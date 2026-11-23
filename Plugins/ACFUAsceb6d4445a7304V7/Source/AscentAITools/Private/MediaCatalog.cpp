// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "MediaCatalog.h"

#include "MediaGateway.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/CriticalSection.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "AscentMediaCatalog"

namespace
{
	FCriticalSection GCatalogLock;
	// Immutable once published: readers share it, a fetch swaps in a new one under the lock.
	TSharedRef<const FACFMediaCatalog::FSnapshot> GCatalogSnapshot = MakeShared<const FACFMediaCatalog::FSnapshot>();
	FACFMediaCatalog::FACFMediaCatalogChanged GCatalogChanged;

	// One fetch at a time: a second Refresh while one runs coalesces into it.
	bool bCatalogFetchInFlight = false;
	bool bCatalogForcePending = false;

	// Grows on every issued fetch; an answer from a generation that already ended is dropped.
	int64 GCatalogGeneration = 0;

	// A Ready snapshot is reused for this long before a Refresh without bForce fetches again.
	constexpr double CatalogCacheSeconds = 5.0 * 60.0;

	TUniqueFunction<void(const FString& Url, const FString& Key, TFunction<void(int32, const FString&)> Done)> GCatalogFetchOverride;

	/** Bridges the gateway holder's change delegate to the catalog, on first use, so static init order cannot matter. */
	struct FCatalogGatewayBridge
	{
		FDelegateHandle Handle;
		FCatalogGatewayBridge()
		{
			Handle = FACFMediaGateway::OnChanged().AddLambda([]()
			{
				FACFMediaCatalog::HandleGatewayChanged();
			});
		}
	};

	void EnsureGatewayBridge()
	{
		static const FCatalogGatewayBridge Bridge;
	}

	TSharedPtr<FJsonObject> ParseCatalogObject(const FString& Body)
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		TSharedPtr<FJsonValue> Value;
		if (FJsonSerializer::Deserialize(Reader, Value) && Value.IsValid() && Value->Type == EJson::Object)
		{
			return Value->AsObject();
		}
		return nullptr;
	}

	const TSharedPtr<FJsonObject>* ChildObject(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* Result = nullptr;
		if (Json)
		{
			Json->TryGetObjectField(Key, Result);
		}
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* ChildArray(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* Result = nullptr;
		if (Json)
		{
			Json->TryGetArrayField(Key, Result);
		}
		return Result;
	}

	// A null or an absent number reads as "not reported"; a number is taken as whole credits.
	bool CreditsOrNothing(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key, int64& OutCredits)
	{
		OutCredits = 0;
		if (!Json)
		{
			return false;
		}
		const TSharedPtr<FJsonValue> Value = Json->TryGetField(Key);
		return Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(OutCredits);
	}

	bool BoolOr(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key, const bool Default)
	{
		bool Value = false;
		return Json && Json->TryGetBoolField(Key, Value) ? Value : Default;
	}

	FString StringOr(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key, const TCHAR* Fallback = TEXT(""))
	{
		FString Value;
		if (Json && Json->TryGetStringField(Key, Value) && !Value.IsEmpty())
		{
			return Value;
		}
		return Fallback;
	}

	/** One failure state: everything empty, only the plain reason left. Prices must never outlive their failure. */
	FACFMediaCatalog::FSnapshot FailedSnapshot(const FString& Error)
	{
		FACFMediaCatalog::FSnapshot Result;
		Result.State = EACFMediaCatalogState::Failed;
		Result.ErrorText = Error;
		return Result;
	}

	/** The plain message for a response the catalog could not serve. */
	FString FailureTextFor(const int32 HttpCode)
	{
		if (HttpCode == 401 || HttpCode == 403)
		{
			return TEXT("Your Yarin sign-in has expired. Sign in again to see your prices.");
		}
		if (HttpCode == 0)
		{
			return TEXT("Yarin could not reach the price service. Check your connection and try again.");
		}
		return FString::Printf(TEXT("The Yarin price service answered HTTP %d. Try again in a moment."), HttpCode);
	}

	/** Folds one answer into the published snapshot and refires when a forced refresh asked while it ran. */
	void OnCatalogResponse(int64 Generation, int32 HttpCode, const FString& Body);
}

bool operator==(const FACFCatalogLine& A, const FACFCatalogLine& B)
{
	return A.Id == B.Id && A.Label == B.Label && A.Credits == B.Credits;
}

bool operator==(const FACFCatalogProvider& A, const FACFCatalogProvider& B)
{
	return A.Id == B.Id && A.Name == B.Name && A.bAvailable == B.bAvailable && A.Lines == B.Lines;
}

bool operator==(const FACFCatalogVoiceModel& A, const FACFCatalogVoiceModel& B)
{
	return A.Id == B.Id && A.Name == B.Name && A.Detail == B.Detail && A.CreditsPer1kChars == B.CreditsPer1kChars;
}

bool operator==(const FACFCatalogVoice& A, const FACFCatalogVoice& B)
{
	return A.Id == B.Id && A.Name == B.Name && A.Description == B.Description && A.PreviewUrl == B.PreviewUrl;
}

bool operator==(const FACFMediaCatalogSnapshot& A, const FACFMediaCatalogSnapshot& B)
{
	return A.State == B.State
		&& A.ErrorText == B.ErrorText
		&& A.Tier == B.Tier
		&& A.bHasCap == B.bHasCap
		&& A.CapCredits == B.CapCredits
		&& A.SpendCredits == B.SpendCredits
		&& A.bHasRemaining == B.bHasRemaining
		&& A.RemainingCredits == B.RemainingCredits
		&& A.bBlocked == B.bBlocked
		&& A.bThreeDEntitled == B.bThreeDEntitled
		&& A.bVoiceEntitled == B.bVoiceEntitled
		&& A.RequiredPlan == B.RequiredPlan
		&& A.ThreeDProviders == B.ThreeDProviders
		&& A.VoiceModels == B.VoiceModels
		&& A.Voices == B.Voices;
}

bool FACFMediaCatalog::ParseCatalogJson(const FString& Body, FSnapshot& OutSnapshot, FString& OutError)
{
	OutSnapshot = FSnapshot();
	OutError.Reset();

	const TSharedPtr<FJsonObject> Root = ParseCatalogObject(Body);
	if (!Root)
	{
		OutError = TEXT("The Yarin service sent an unreadable price catalog.");
		return false;
	}

	// Unknown extra fields are tolerated; absent optional fields keep their defaults.
	if (const TSharedPtr<FJsonObject>* Plan = ChildObject(Root, TEXT("plan")))
	{
		OutSnapshot.Tier = StringOr(*Plan, TEXT("tier"));
		OutSnapshot.bHasCap = CreditsOrNothing(*Plan, TEXT("capCredits"), OutSnapshot.CapCredits);
		CreditsOrNothing(*Plan, TEXT("spendCredits"), OutSnapshot.SpendCredits);
		OutSnapshot.bHasRemaining = CreditsOrNothing(*Plan, TEXT("remainingCredits"), OutSnapshot.RemainingCredits);
		OutSnapshot.bBlocked = BoolOr(*Plan, TEXT("blocked"), false);
	}
	OutSnapshot.RequiredPlan = StringOr(Root, TEXT("requiredPlan"));

	if (const TSharedPtr<FJsonObject>* ThreeD = ChildObject(Root, TEXT("threeD")))
	{
		OutSnapshot.bThreeDEntitled = BoolOr(*ThreeD, TEXT("entitled"), false);
		if (const TArray<TSharedPtr<FJsonValue>>* Providers = ChildArray(*ThreeD, TEXT("providers")))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Providers)
			{
				if (!Value.IsValid() || Value->Type != EJson::Object) { continue; }
				const TSharedPtr<FJsonObject> ProviderJson = Value->AsObject();
				FACFCatalogProvider Provider;
				Provider.Id = StringOr(ProviderJson, TEXT("id"));
				Provider.Name = StringOr(ProviderJson, TEXT("name"), *Provider.Id);
				Provider.bAvailable = BoolOr(ProviderJson, TEXT("available"), true);
				if (const TArray<TSharedPtr<FJsonValue>>* Lines = ChildArray(ProviderJson, TEXT("lines")))
				{
					for (const TSharedPtr<FJsonValue>& LineValue : *Lines)
					{
						if (!LineValue.IsValid() || LineValue->Type != EJson::Object) { continue; }
						const TSharedPtr<FJsonObject> LineJson = LineValue->AsObject();
						FACFCatalogLine Line;
						Line.Id = StringOr(LineJson, TEXT("id"));
						Line.Label = StringOr(LineJson, TEXT("label"));
						CreditsOrNothing(LineJson, TEXT("credits"), Line.Credits);
						Provider.Lines.Add(MoveTemp(Line));
					}
				}
				OutSnapshot.ThreeDProviders.Add(MoveTemp(Provider));
			}
		}
	}

	if (const TSharedPtr<FJsonObject>* Voice = ChildObject(Root, TEXT("voice")))
	{
		OutSnapshot.bVoiceEntitled = BoolOr(*Voice, TEXT("entitled"), false);
		if (const TArray<TSharedPtr<FJsonValue>>* Models = ChildArray(*Voice, TEXT("models")))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Models)
			{
				if (!Value.IsValid() || Value->Type != EJson::Object) { continue; }
				const TSharedPtr<FJsonObject> ModelJson = Value->AsObject();
				FACFCatalogVoiceModel Model;
				Model.Id = StringOr(ModelJson, TEXT("id"));
				Model.Name = StringOr(ModelJson, TEXT("name"), *Model.Id);
				Model.Detail = StringOr(ModelJson, TEXT("detail"));
				CreditsOrNothing(ModelJson, TEXT("creditsPer1kChars"), Model.CreditsPer1kChars);
				OutSnapshot.VoiceModels.Add(MoveTemp(Model));
			}
		}
		if (const TArray<TSharedPtr<FJsonValue>>* Voices = ChildArray(*Voice, TEXT("voices")))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Voices)
			{
				if (!Value.IsValid() || Value->Type != EJson::Object) { continue; }
				const TSharedPtr<FJsonObject> VoiceJson = Value->AsObject();
				FACFCatalogVoice VoiceEntry;
				VoiceEntry.Id = StringOr(VoiceJson, TEXT("id"));
				VoiceEntry.Name = StringOr(VoiceJson, TEXT("name"));
				VoiceEntry.Description = StringOr(VoiceJson, TEXT("description"));
				// The preview url is a string or an explicit null; null reads as none.
				VoiceEntry.PreviewUrl = StringOr(VoiceJson, TEXT("previewUrl"));
				OutSnapshot.Voices.Add(MoveTemp(VoiceEntry));
			}
		}
	}

	return true;
}

FString FACFMediaCatalog::FormatCredits(const int64 Credits)
{
	const int64 Whole = FMath::Max<int64>(Credits, 0);
	const FString Digits = LexToString(Whole);
	FString Grouped;
	Grouped.Reserve(Digits.Len() + Digits.Len() / 3 + 8);
	for (int32 Index = 0; Index < Digits.Len(); ++Index)
	{
		if (Index > 0 && (Digits.Len() - Index) % 3 == 0)
		{
			Grouped.AppendChar(TEXT(','));
		}
		Grouped.AppendChar(Digits[Index]);
	}
	Grouped.Append(Whole == 1 ? TEXT(" credit") : TEXT(" credits"));
	return Grouped;
}

FString FACFMediaCatalog::FormatAboutCredits(const int64 Credits)
{
	return TEXT("about ") + FormatCredits(Credits);
}

FString FACFMediaCatalog::FormatCreditsLeft(const int64 Credits)
{
	return FormatCredits(Credits) + TEXT(" left");
}

FString FACFMediaCatalog::FormatCreditsLeftThisMonth(const int64 Credits)
{
	return FormatCreditsLeft(Credits) + TEXT(" this month");
}

FACFYarinChipState ComputeYarinChipState(const bool bConnected, const FACFMediaCatalogSnapshot& Snapshot)
{
	FACFYarinChipState Chip;
	Chip.bConnected = bConnected;
	if (!bConnected)
	{
		// An invite to connect: no amount, no price of any kind while signed out.
		Chip.Text = TEXT("Connect Yarin");
		return Chip;
	}
	Chip.bShowsBalance = Snapshot.State == EACFMediaCatalogState::Ready && Snapshot.bHasRemaining;
	Chip.Text = Chip.bShowsBalance
		? FString::Printf(TEXT("Yarin  %s"), *FACFMediaCatalog::FormatCreditsLeft(Snapshot.RemainingCredits))
		: TEXT("Yarin");
	return Chip;
}

const FACFCatalogLine* FACFMediaCatalog::FindLine(const FSnapshot& Snapshot, const FString& ProviderId, const FString& LineId)
{
	for (const FACFCatalogProvider& Provider : Snapshot.ThreeDProviders)
	{
		if (Provider.Id != ProviderId)
		{
			continue;
		}
		for (const FACFCatalogLine& Line : Provider.Lines)
		{
			if (Line.Id == LineId)
			{
				return &Line;
			}
		}
		return nullptr;
	}
	return nullptr;
}

TSharedRef<const FACFMediaCatalog::FSnapshot> FACFMediaCatalog::GetSnapshot()
{
	EnsureGatewayBridge();
	FScopeLock Lock(&GCatalogLock);
	return GCatalogSnapshot;
}

FACFMediaCatalog::FACFMediaCatalogChanged& FACFMediaCatalog::OnChanged()
{
	return GCatalogChanged;
}

void FACFMediaCatalog::HandleGatewayChanged()
{
	if (!FACFMediaGateway::IsPresent())
	{
		// Sign out must make the prices vanish at once, not after the next fetch.
		FSnapshot Disconnected;
		bool bChanged = false;
		{
			FScopeLock Lock(&GCatalogLock);
			bCatalogFetchInFlight = false;
			// An answer already on the wire belongs to the account that just left: it must not bring its prices back.
			++GCatalogGeneration;
			bCatalogForcePending = false;
			bChanged = !(*GCatalogSnapshot == Disconnected);
			GCatalogSnapshot = MakeShared<const FSnapshot>(MoveTemp(Disconnected));
		}
		// Broadcast outside the lock: a subscriber reads the snapshot it just changed.
		if (bChanged)
		{
			GCatalogChanged.Broadcast();
		}
		return;
	}
	// A new credential belongs to a different account: the old snapshot (with its prices) is stale this instant.
	FSnapshot Loading;
	Loading.State = EACFMediaCatalogState::Loading;
	bool bChanged = false;
	{
		FScopeLock Lock(&GCatalogLock);
		bChanged = !(*GCatalogSnapshot == Loading);
		GCatalogSnapshot = MakeShared<const FSnapshot>(MoveTemp(Loading));
	}
	if (bChanged)
	{
		GCatalogChanged.Broadcast();
	}
	Refresh(true);
}

void FACFMediaCatalog::NotifyJobFinished()
{
	// The gateway just charged the account: the cached balance is outdated by definition.
	Refresh(true);
}

void FACFMediaCatalog::Refresh(const bool bForce)
{
	EnsureGatewayBridge();
	const FACFMediaGateway::FCredential Credential = FACFMediaGateway::GetCredential();
	if (Credential.BaseUrl.IsEmpty() || Credential.Key.IsEmpty())
	{
		HandleGatewayChanged();
		return;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	{
		FScopeLock Lock(&GCatalogLock);
		if (bCatalogFetchInFlight)
		{
			// Coalesce: the running fetch answers, and a forced one that arrived in the meantime refetches.
			bCatalogForcePending |= bForce;
			return;
		}
		if (!bForce
			&& GCatalogSnapshot->State == EACFMediaCatalogState::Ready
			&& NowSeconds - GCatalogSnapshot->FetchedAtSeconds < CatalogCacheSeconds)
		{
			return;
		}
		bCatalogFetchInFlight = true;
		++GCatalogGeneration;
	}

	const FString Url = FACFMediaGateway::GetMediaRoot() + TEXT("/catalog");
	const int64 Generation = GCatalogGeneration;
	TFunction<void(int32, const FString&)> Done = [Generation](const int32 HttpCode, const FString& Body)
	{
		OnCatalogResponse(Generation, HttpCode, Body);
	};
	if (GCatalogFetchOverride)
	{
		GCatalogFetchOverride(Url, Credential.Key, MoveTemp(Done));
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(20.0f);
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	// The key rides the header only; nothing logs or stores it.
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Credential.Key);
	Request->OnProcessRequestComplete().BindLambda([Generation](FHttpRequestPtr, const FHttpResponsePtr Response, const bool bConnected)
	{
		OnCatalogResponse(Generation, bConnected && Response.IsValid() ? Response->GetResponseCode() : 0,
			Response.IsValid() ? Response->GetContentAsString() : FString());
	});
	if (!Request->ProcessRequest())
	{
		// The request could not even start: fail the snapshot now instead of waiting forever in flight.
		OnCatalogResponse(Generation, 0, FString());
	}
}

namespace
{
	/** Folds one answer into the published snapshot and refires when a forced refresh asked while it ran. */
	void OnCatalogResponse(const int64 Generation, const int32 HttpCode, const FString& Body)
	{
		FACFMediaCatalog::FSnapshot Next;
		bool bFetchAgain = false;
		bool bChanged = false;
		{
			FScopeLock Lock(&GCatalogLock);
			if (Generation != GCatalogGeneration)
			{
				// A newer fetch owns the snapshot now (the credential changed underneath this answer).
				return;
			}
			bCatalogFetchInFlight = false;
			bFetchAgain = bCatalogForcePending;
			bCatalogForcePending = false;

			if (HttpCode == 200)
			{
				FString ParseError;
				if (FACFMediaCatalog::ParseCatalogJson(Body, Next, ParseError))
				{
					Next.State = EACFMediaCatalogState::Ready;
					Next.FetchedAtSeconds = FPlatformTime::Seconds();
				}
				else
				{
					Next = FailedSnapshot(ParseError);
				}
			}
			else
			{
				Next = FailedSnapshot(FailureTextFor(HttpCode));
			}

			bChanged = !(*GCatalogSnapshot == Next);
			GCatalogSnapshot = MakeShared<const FACFMediaCatalog::FSnapshot>(MoveTemp(Next));
		}
		// Broadcast outside the lock: a subscriber reads the snapshot it just changed.
		if (bChanged)
		{
			GCatalogChanged.Broadcast();
		}
		// A forced refresh asked for while the fetch ran (NotifyJobFinished, a card opening) is honoured now.
		if (bFetchAgain)
		{
			FACFMediaCatalog::Refresh(true);
		}
	}
}

void FACFMediaCatalog::SetFetchOverride(TUniqueFunction<void(const FString& Url, const FString& Key,
	TFunction<void(int32, const FString&)> Done)> InOverride)
{
	GCatalogFetchOverride = MoveTemp(InOverride);
}

#undef LOCTEXT_NAMESPACE
