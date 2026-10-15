// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreeDJob.h"

/**
 * One WaveSpeed (API v3) generation, by default on the Hunyuan3D v3 models. Text or image in, GLB out. No rigging.
 *
 *   image:  POST /media/uploads (returns a signed PUT target and a public download_url) -> PUT the PNG there
 *   submit: POST /{model}  with image (the download_url) or prompt
 *   poll:   GET /predictions/{id}/result -> download the GLB from outputs
 */
class FACF3DWaveSpeedJob final : public FACF3DJob
{
public:
	FACF3DWaveSpeedJob(FACF3DRequest InRequest, bool bInFromImage);

protected:
	virtual void Begin() override;

private:
	void UploadImage();
	void Submit(const FString& ImageUrl);
	void OnSubmitted(int32 HttpCode, const TSharedPtr<FJsonObject>& Json);
	void OnPollResponse(int32 HttpCode, const TSharedPtr<FJsonObject>& Json);
	FString Describe(int32 HttpCode, const TSharedPtr<FJsonObject>& Json) const;

	FACF3DRequest Request;
	bool bFromImage = false;

	FString BaseUrl;
	FString ModelId;
	bool bTextures = true;
	bool bPbr = true;
	int32 FaceCount = 0;

	FString PredictionId;
};
