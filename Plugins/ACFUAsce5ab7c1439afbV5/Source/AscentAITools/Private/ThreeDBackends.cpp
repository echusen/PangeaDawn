// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ThreeDBackend.h"

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
