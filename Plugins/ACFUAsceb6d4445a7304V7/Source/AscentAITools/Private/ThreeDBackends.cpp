// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ThreeDBackend.h"

#include "MediaGateway.h"
#include "MeshyClient.h"
#include "TripoClient.h"
#include "WaveSpeedClient.h"

#define LOCTEXT_NAMESPACE "ACF3DBackends"

namespace
{
	class FMeshyBackend final : public IACF3DBackend
	{
	public:
		virtual EACF3DProvider GetProvider() const override { return EACF3DProvider::Meshy; }
		virtual FText GetDisplayName() const override { return LOCTEXT("Meshy", "Meshy"); }

		virtual FACF3DCapabilities GetCapabilities() const override
		{
			FACF3DCapabilities Caps;
			Caps.bTextTo3D = true;
			Caps.bImageTo3D = true;
			Caps.bRigging = true;
			Caps.bArtStyle = true;
			Caps.bTopology = true;
			Caps.bRefine = true;
			return Caps;
		}

		virtual void TextTo3D(FACF3DRequest Request, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished) const override
		{
			FACFMeshyClient::TextTo3D(Request.Prompt, Request.bRig, Request.HeightMeters, MoveTemp(OnStatus), MoveTemp(OnFinished));
		}

		virtual void ImageTo3D(FACF3DRequest Request, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished) const override
		{
			FACFMeshyClient::ImageTo3D(Request.PngBytes, Request.bRig, Request.HeightMeters, MoveTemp(OnStatus), MoveTemp(OnFinished));
		}
	};

	class FTripoBackend final : public IACF3DBackend
	{
	public:
		virtual EACF3DProvider GetProvider() const override { return EACF3DProvider::Tripo; }
		virtual FText GetDisplayName() const override { return LOCTEXT("Tripo", "Tripo"); }

		virtual FACF3DCapabilities GetCapabilities() const override
		{
			FACF3DCapabilities Caps;
			Caps.bTextTo3D = true;
			Caps.bImageTo3D = true;
			Caps.bRigging = true;
			return Caps;
		}

		virtual void TextTo3D(FACF3DRequest Request, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished) const override
		{
			MakeShared<FACF3DTripoJob>(MoveTemp(Request), /*bFromImage*/ false)->Run(MoveTemp(OnStatus), MoveTemp(OnFinished));
		}

		virtual void ImageTo3D(FACF3DRequest Request, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished) const override
		{
			MakeShared<FACF3DTripoJob>(MoveTemp(Request), /*bFromImage*/ true)->Run(MoveTemp(OnStatus), MoveTemp(OnFinished));
		}
	};

	class FWaveSpeedBackend final : public IACF3DBackend
	{
	public:
		virtual EACF3DProvider GetProvider() const override { return EACF3DProvider::WaveSpeed; }
		virtual FText GetDisplayName() const override { return LOCTEXT("WaveSpeed", "WaveSpeed"); }

		virtual FACF3DCapabilities GetCapabilities() const override
		{
			FACF3DCapabilities Caps;
			Caps.bTextTo3D = true;
			Caps.bImageTo3D = true;
			return Caps;
		}

		virtual void TextTo3D(FACF3DRequest Request, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished) const override
		{
			MakeShared<FACF3DWaveSpeedJob>(MoveTemp(Request), /*bFromImage*/ false)->Run(MoveTemp(OnStatus), MoveTemp(OnFinished));
		}

		virtual void ImageTo3D(FACF3DRequest Request, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished) const override
		{
			MakeShared<FACF3DWaveSpeedJob>(MoveTemp(Request), /*bFromImage*/ true)->Run(MoveTemp(OnStatus), MoveTemp(OnFinished));
		}
	};
}

namespace ACF3D
{
	namespace
	{
		/** The provider slug the Yarin gateway's media3d route uses. */
		const TCHAR* ProviderSlug(EACF3DProvider Provider)
		{
			switch (Provider)
			{
			case EACF3DProvider::Tripo: return TEXT("tripo");
			case EACF3DProvider::WaveSpeed: return TEXT("wavespeed");
			case EACF3DProvider::Meshy:
			default: return TEXT("meshy");
			}
		}

		/** The provider's own path below its host, taken from the configured base URL: /v3, /api/v3, or empty. */
		FString NativePathSuffixOf(const FString& ConfiguredBaseUrl)
		{
			const int32 SchemeEnd = ConfiguredBaseUrl.Find(TEXT("://"), ESearchCase::IgnoreCase);
			if (SchemeEnd == INDEX_NONE)
			{
				return FString();
			}
			int32 PathStart = INDEX_NONE;
			for (int32 Index = SchemeEnd + 3; Index < ConfiguredBaseUrl.Len(); ++Index)
			{
				if (ConfiguredBaseUrl[Index] == TEXT('/'))
				{
					PathStart = Index;
					break;
				}
			}
			if (PathStart == INDEX_NONE)
			{
				return FString();
			}
			FString Path = ConfiguredBaseUrl.Mid(PathStart);
			while (Path.EndsWith(TEXT("/")))
			{
				Path.LeftChopInline(1);
			}
			return Path;
		}

		FACF3DEndpoints OwnProviderEndpoints(const UACFAIToolsSettings& Settings, EACF3DProvider Provider,
			const FString& OwnKey)
		{
			FACF3DEndpoints Result;
			Result.BaseUrl = Settings.GetProviderBaseUrl(Provider).TrimStartAndEnd();
			Result.BaseUrl.RemoveFromEnd(TEXT("/"));
			Result.ApiKey = OwnKey;
			Result.bBilledToYarin = false;
			return Result;
		}
	}

	FACF3DEndpoints ResolveEndpoints(EACF3DProvider Provider, FString* OutError, const UACFAIToolsSettings* Settings)
	{
		const auto SetError = [OutError](const FString& Text)
		{
			if (OutError)
			{
				*OutError = Text;
			}
			return FACF3DEndpoints();
		};
		const auto ClearError = [OutError]()
		{
			if (OutError)
			{
				*OutError = FString();
			}
		};

		if (!Settings)
		{
			Settings = UACFAIToolsSettings::Get();
		}
		if (!Settings)
		{
			return SetError(TEXT("The 3D settings are not available yet. Try again."));
		}
		ClearError();

		const FString OwnKey = Settings->GetThreeDApiKey(Provider).TrimStartAndEnd();
		const bool bWantYarin = Settings->ThreeDKeySource == EACF3DKeySource::Yarin
			|| (Settings->ThreeDKeySource == EACF3DKeySource::Auto && OwnKey.IsEmpty());
		if (!bWantYarin)
		{
			if (OwnKey.IsEmpty())
			{
				return SetError(FString::Printf(TEXT("Your %s API key is not set. Add it in the Yarin gear settings or Project Settings > Plugins > Ascent AI Tools."),
					*GetBackend(Provider).GetDisplayName().ToString()));
			}
			FACF3DEndpoints Result = OwnProviderEndpoints(*Settings, Provider, OwnKey);
			if (Result.BaseUrl.IsEmpty())
			{
				return SetError(FString::Printf(TEXT("The %s base URL is empty. Check Project Settings > Plugins > Ascent AI Tools > Advanced."),
					*GetBackend(Provider).GetDisplayName().ToString()));
			}
			return Result;
		}

		if (!FACFMediaGateway::IsPresent())
		{
			// OwnKey is empty here: Yarin was asked for explicitly or through Auto.
			return SetError(FString::Printf(TEXT("Sign in to Yarin to generate meshes on your Yarin credit, or set your own %s API key."),
				*GetBackend(Provider).GetDisplayName().ToString()));
		}
		const FACFMediaGateway::FCredential Credential = FACFMediaGateway::GetCredential();

		// The configured base URL contributes the provider's native path to the gateway route, so it must be
		// something a URL and a header can carry.
		const FString Configured = Settings->GetProviderBaseUrl(Provider).TrimStartAndEnd();
		if (!FACFMediaGateway::IsUsableGatewayUrl(Configured))
		{
			return SetError(FString::Printf(TEXT("The %s base URL is not a usable web address. Check Project Settings > Plugins > Ascent AI Tools > Advanced."),
				*GetBackend(Provider).GetDisplayName().ToString()));
		}

		FACF3DEndpoints Result;
		Result.BaseUrl = FACFMediaGateway::GetMediaRoot() + TEXT("/") + ProviderSlug(Provider) + NativePathSuffixOf(Configured);
		Result.ApiKey = Credential.Key;
		Result.bBilledToYarin = true;
		return Result;
	}

	const IACF3DBackend& GetBackend(EACF3DProvider Provider)
	{
		static const FMeshyBackend Meshy;
		static const FTripoBackend Tripo;
		static const FWaveSpeedBackend WaveSpeed;
		switch (Provider)
		{
		case EACF3DProvider::Tripo: return Tripo;
		case EACF3DProvider::WaveSpeed: return WaveSpeed;
		case EACF3DProvider::Meshy:
		default: return Meshy;
		}
	}

	const IACF3DBackend& GetActiveBackend()
	{
		return GetBackend(UACFAIToolsSettings::Get()->ThreeDProvider);
	}
}

#undef LOCTEXT_NAMESPACE
