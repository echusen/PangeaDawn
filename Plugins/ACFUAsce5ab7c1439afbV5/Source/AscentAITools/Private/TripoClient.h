// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreeDJob.h"

/**
 * One Tripo (API v3) generation. Text or image in, GLB out. With bRig the finished mesh is sent to Tripo's
 * auto rigger and the rigged FBX comes back instead.
 *
 *   image:  POST /files (multipart, returns a file_token) -> POST /generation/image-to-model
 *   text:   POST /generation/text-to-model
 *   both:   poll GET /tasks/{id} -> download output.model_url
 *   rig:    POST /animations/rig {input: generation task id} -> poll -> download the FBX
 */
class FACF3DTripoJob final : public FACF3DJob
{
public:
	FACF3DTripoJob(FACF3DRequest InRequest, bool bInFromImage);

protected:
	virtual void Begin() override;

private:
	void UploadImage();
	void CreateGeneration(const FString& ImageToken);
	void StartRig();
	/** Reads task_id from a create response, then starts polling that task. */
	void OnTaskCreated(int32 HttpCode, const TSharedPtr<FJsonObject>& Json);
	void OnPollResponse(int32 HttpCode, const TSharedPtr<FJsonObject>& Json);
	FString Describe(int32 HttpCode, const TSharedPtr<FJsonObject>& Json) const;

	FACF3DRequest Request;
	bool bFromImage = false;
	bool bWantRig = false;
	bool bRigStage = false;

	FString BaseUrl;
	FString ModelVersion;
	bool bTextures = true;
	bool bPbr = true;
	int32 FaceLimit = 0;

	FString GenerationTaskId;
	FString ActiveTaskId;
};
