// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ThreeDParsers.h"

namespace ACF3D
{
	namespace
	{
		/** Rate limits and server side hiccups: the task may still be fine, so the caller keeps polling. */
		bool IsTransientHttp(int32 HttpCode)
		{
			return HttpCode <= 0 || HttpCode == 408 || HttpCode == 425 || HttpCode == 429 || HttpCode >= 500;
		}

		bool IsHttpOk(int32 HttpCode)
		{
			return HttpCode >= 200 && HttpCode < 300;
		}

		FPollResult MakeFailed(const FString& Error)
		{
			FPollResult Result;
			Result.State = EPollState::Failed;
			Result.Error = Error;
			return Result;
		}

		/** Classifies a non success transport status. Fills OutResult (Pending when transient, Failed otherwise) and returns true. Returns false when the status is fine. */
		bool TryClassifyTransport(int32 HttpCode, const TSharedPtr<FJsonObject>& Json, FPollResult& OutResult)
		{
			if (IsHttpOk(HttpCode))
			{
				return false;
			}
			OutResult = IsTransientHttp(HttpCode) ? FPollResult() : MakeFailed(ExtractServiceError(Json, HttpCode));
			return true;
		}

		void CollectUrls(const TSharedPtr<FJsonValue>& Value, TArray<FString>& OutUrls)
		{
			if (!Value.IsValid())
			{
				return;
			}
			switch (Value->Type)
			{
			case EJson::String:
			{
				const FString Text = Value->AsString();
				if (Text.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) || Text.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
				{
					OutUrls.Add(Text);
				}
				break;
			}
			case EJson::Array:
				for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
				{
					CollectUrls(Item, OutUrls);
				}
				break;
			case EJson::Object:
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Value->AsObject()->Values)
				{
					CollectUrls(Field.Value, OutUrls);
				}
				break;
			default:
				break;
			}
		}

		FString LowerStatus(const TSharedPtr<FJsonObject>& Data)
		{
			FString Status;
			Data->TryGetStringField(TEXT("status"), Status);
			return Status.ToLower();
		}
	}

	FString ExtractServiceError(const TSharedPtr<FJsonObject>& Json, int32 HttpCode)
	{
		if (Json.IsValid())
		{
			FString Text;
			for (const TCHAR* Field : {TEXT("message"), TEXT("error"), TEXT("suggestion")})
			{
				if (Json->TryGetStringField(Field, Text) && !Text.TrimStartAndEnd().IsEmpty())
				{
					return Text;
				}
			}
		}
		return FString::Printf(TEXT("The 3D service returned HTTP %d."), HttpCode);
	}

	FPollResult ParseTripoTask(int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
	{
		FPollResult Result;
		if (TryClassifyTransport(HttpCode, Json, Result))
		{
			return Result;
		}
		if (!Json.IsValid())
		{
			return FPollResult();
		}

		int32 BodyCode = 0;
		if (Json->TryGetNumberField(TEXT("code"), BodyCode) && BodyCode != 0)
		{
			return MakeFailed(ExtractServiceError(Json, HttpCode));
		}

		const TSharedPtr<FJsonObject>* DataPtr = nullptr;
		if (!Json->TryGetObjectField(TEXT("data"), DataPtr) || !DataPtr || !DataPtr->IsValid())
		{
			return FPollResult();
		}
		const TSharedPtr<FJsonObject>& Data = *DataPtr;

		int32 Progress = -1;
		if (Data->TryGetNumberField(TEXT("progress"), Progress))
		{
			Progress = FMath::Clamp(Progress, 0, 100);
		}

		const FString Status = LowerStatus(Data);
		if (Status == TEXT("queued") || Status == TEXT("running"))
		{
			Result.Progress = Progress;
			return Result;
		}
		if (Status == TEXT("success"))
		{
			const TSharedPtr<FJsonObject>* OutputPtr = nullptr;
			FString ModelUrl;
			if (Data->TryGetObjectField(TEXT("output"), OutputPtr) && OutputPtr && OutputPtr->IsValid())
			{
				(*OutputPtr)->TryGetStringField(TEXT("model_url"), ModelUrl);
			}
			if (ModelUrl.IsEmpty())
			{
				return MakeFailed(TEXT("Tripo finished but returned no model file."));
			}
			Result.State = EPollState::Succeeded;
			Result.Progress = 100;
			Result.ModelUrl = ModelUrl;
			return Result;
		}
		if (Status == TEXT("failed") || Status == TEXT("cancelled") || Status == TEXT("banned") || Status == TEXT("expired"))
		{
			FString Message;
			if (!Data->TryGetStringField(TEXT("error_message"), Message) || Message.TrimStartAndEnd().IsEmpty())
			{
				Message = FString::Printf(TEXT("Tripo generation ended with status %s."), *Status);
			}
			return MakeFailed(Message);
		}
		return Result;
	}

	FPollResult ParseWaveSpeedResult(int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
	{
		FPollResult Result;
		if (TryClassifyTransport(HttpCode, Json, Result))
		{
			return Result;
		}
		if (!Json.IsValid())
		{
			return FPollResult();
		}

		int32 BodyCode = 200;
		if (Json->TryGetNumberField(TEXT("code"), BodyCode) && (BodyCode < 200 || BodyCode >= 300))
		{
			return IsTransientHttp(BodyCode) ? FPollResult() : MakeFailed(ExtractServiceError(Json, BodyCode));
		}

		// The result is wrapped in "data", but an unwrapped task object is accepted too.
		const TSharedPtr<FJsonObject>* DataPtr = nullptr;
		const TSharedPtr<FJsonObject> Data = (Json->TryGetObjectField(TEXT("data"), DataPtr) && DataPtr && DataPtr->IsValid()) ? *DataPtr : Json;

		const FString Status = LowerStatus(Data);
		if (Status == TEXT("completed"))
		{
			const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
			FString ModelUrl;
			if (Data->TryGetArrayField(TEXT("outputs"), Outputs) && Outputs)
			{
				ModelUrl = PickModelUrl(*Outputs);
			}
			if (ModelUrl.IsEmpty())
			{
				return MakeFailed(TEXT("WaveSpeed finished but returned no GLB model file."));
			}
			Result.State = EPollState::Succeeded;
			Result.Progress = 100;
			Result.ModelUrl = ModelUrl;
			return Result;
		}
		if (Status == TEXT("failed") || Status == TEXT("cancelled") || Status == TEXT("timeout") || Status == TEXT("deleted"))
		{
			FString Message;
			if (!Data->TryGetStringField(TEXT("error"), Message) || Message.TrimStartAndEnd().IsEmpty())
			{
				Message = FString::Printf(TEXT("WaveSpeed generation ended with status %s."), *Status);
			}
			return MakeFailed(Message);
		}
		return Result;
	}

	FString ExtensionFromUrl(const FString& Url)
	{
		FString Path = Url;
		int32 Cut = INDEX_NONE;
		if (Path.FindChar(TEXT('?'), Cut))
		{
			Path.LeftInline(Cut);
		}
		if (Path.FindChar(TEXT('#'), Cut))
		{
			Path.LeftInline(Cut);
		}
		int32 Slash = INDEX_NONE;
		const FString Name = Path.FindLastChar(TEXT('/'), Slash) ? Path.Mid(Slash + 1) : Path;
		int32 Dot = INDEX_NONE;
		if (!Name.FindLastChar(TEXT('.'), Dot))
		{
			return FString();
		}
		return Name.Mid(Dot + 1).ToLower();
	}

	FString PickModelUrl(const TArray<TSharedPtr<FJsonValue>>& Outputs)
	{
		TArray<FString> Urls;
		for (const TSharedPtr<FJsonValue>& Item : Outputs)
		{
			CollectUrls(Item, Urls);
		}
		for (const TCHAR* Wanted : {TEXT("glb"), TEXT("gltf")})
		{
			for (const FString& Url : Urls)
			{
				if (ExtensionFromUrl(Url) == Wanted)
				{
					return Url;
				}
			}
		}
		return FString();
	}

	void BuildMultipartFile(const FString& Boundary, const FString& FieldName, const FString& FileName, const FString& ContentType, const TArray<uint8>& Bytes, TArray<uint8>& OutBody)
	{
		const FTCHARToUTF8 Head(*FString::Printf(
			TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n"),
			*Boundary, *FieldName, *FileName, *ContentType));
		const FTCHARToUTF8 Tail(*FString::Printf(TEXT("\r\n--%s--\r\n"), *Boundary));

		OutBody.Reset(Head.Length() + Bytes.Num() + Tail.Length());
		OutBody.Append(reinterpret_cast<const uint8*>(Head.Get()), Head.Length());
		OutBody.Append(Bytes);
		OutBody.Append(reinterpret_cast<const uint8*>(Tail.Get()), Tail.Length());
	}

	int32 ClampWaveSpeedFaceCount(int32 Requested)
	{
		return FMath::Clamp(Requested, 40000, 1500000);
	}
}
