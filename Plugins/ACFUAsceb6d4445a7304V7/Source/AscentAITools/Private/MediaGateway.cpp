// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "MediaGateway.h"

#include "HAL/CriticalSection.h"

namespace
{
	// One writer at a time: the assistant sets or clears on the game thread while jobs read it.
	FCriticalSection GGatewayLock;
	FACFMediaGateway::FCredential GCredential;
	FACFMediaGatewayChangedDelegate GCredentialChanged;

	/** The scheme and host of a URL: https://yarin.example.com/litellm/v1 -> https://yarin.example.com. */
	FString OriginOf(const FString& Url)
	{
		const int32 SchemeEnd = Url.Find(TEXT("://"), ESearchCase::IgnoreCase);
		if (SchemeEnd == INDEX_NONE)
		{
			return FString();
		}
		const int32 HostStart = SchemeEnd + 3;
		int32 PathStart = INDEX_NONE;
		for (int32 Index = HostStart; Index < Url.Len(); ++Index)
		{
			if (Url[Index] == TEXT('/'))
			{
				PathStart = Index;
				break;
			}
		}
		return PathStart == INDEX_NONE ? Url : Url.Left(PathStart);
	}
}

bool FACFMediaGateway::Set(const FString& GatewayBaseUrl, const FString& GatewayKey)
{
	if (!IsUsableGatewayUrl(GatewayBaseUrl) || !IsUsableGatewayKey(GatewayKey))
	{
		return false;
	}
	bool bBroadcast = false;
	{
		FScopeLock Lock(&GGatewayLock);
		// A repeated mint with the same values is not a change: only real transitions broadcast.
		bBroadcast = GCredential.BaseUrl != GatewayBaseUrl || GCredential.Key != GatewayKey;
		GCredential.BaseUrl = GatewayBaseUrl;
		GCredential.Key = GatewayKey;
	}
	if (bBroadcast)
	{
		GCredentialChanged.Broadcast();
	}
	return true;
}

void FACFMediaGateway::Clear()
{
	bool bBroadcast = false;
	{
		FScopeLock Lock(&GGatewayLock);
		bBroadcast = !GCredential.BaseUrl.IsEmpty() || !GCredential.Key.IsEmpty();
		GCredential = FCredential();
	}
	if (bBroadcast)
	{
		GCredentialChanged.Broadcast();
	}
}

bool FACFMediaGateway::IsPresent()
{
	FScopeLock Lock(&GGatewayLock);
	return !GCredential.BaseUrl.IsEmpty() && !GCredential.Key.IsEmpty();
}

FACFMediaGateway::FCredential FACFMediaGateway::GetCredential()
{
	FScopeLock Lock(&GGatewayLock);
	return GCredential;
}

FString FACFMediaGateway::GetMediaRoot()
{
	FScopeLock Lock(&GGatewayLock);
	if (GCredential.BaseUrl.IsEmpty())
	{
		return FString();
	}
	return OriginOf(GCredential.BaseUrl) + TEXT("/media3d");
}

bool FACFMediaGateway::IsUsableGatewayUrl(const FString& Url)
{
	constexpr int32 MaxUrlLength = 512;
	if (Url.IsEmpty() || Url.Len() > MaxUrlLength)
	{
		return false;
	}
	// https unless a loopback address, so http links to remote hosts are refused.
	const bool bSecure = Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
	if (!bSecure)
	{
		// A prefix test alone would trust "http://localhost.evil.example": the host must end right after the loopback name.
		bool bLoopback = false;
		for (const TCHAR* Loopback : { TEXT("http://127.0.0.1"), TEXT("http://localhost") })
		{
			const int32 LoopbackLen = FCString::Strlen(Loopback);
			if (!Url.StartsWith(Loopback, ESearchCase::IgnoreCase))
			{
				continue;
			}
			const TCHAR Next = Url.Len() > LoopbackLen ? Url[LoopbackLen] : TEXT('\0');
			bLoopback = Next == TEXT('\0') || Next == TEXT(':') || Next == TEXT('/');
			break;
		}
		if (!bLoopback)
		{
			return false;
		}
	}
	for (const TCHAR Ch : Url)
	{
		// Quotes and backslashes are excluded as well, so the value cannot smuggle headers or escapes.
		if (Ch <= TEXT(' ') || Ch > TEXT('~') || Ch == TEXT('"') || Ch == TEXT('\\'))
		{
			return false;
		}
	}
	return true;
}

bool FACFMediaGateway::IsUsableGatewayKey(const FString& Key)
{
	constexpr int32 MaxKeyLength = 512;
	if (Key.IsEmpty() || Key.Len() > MaxKeyLength)
	{
		return false;
	}
	for (const TCHAR Ch : Key)
	{
		if (Ch <= TEXT(' ') || Ch > TEXT('~'))
		{
			return false;
		}
	}
	return true;
}

FACFMediaGatewayChangedDelegate& FACFMediaGateway::OnChanged()
{
	return GCredentialChanged;
}
