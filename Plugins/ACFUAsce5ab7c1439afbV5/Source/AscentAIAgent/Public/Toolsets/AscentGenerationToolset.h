// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"

#include "AscentGenerationToolset.generated.h"

/// AI asset generation tools backed by the Ascent AI Tools subsystem: image generation and 3D mesh generation (Meshy, Tripo or WaveSpeed, whichever 3D provider the user selected), with automatic import into the project.
UCLASS(BlueprintType, Hidden)
class ASCENTAIAGENT_API UAscentGenerationToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Generates an image with the configured image provider and imports it as a Texture asset. Runs asynchronously; the result arrives when generation finishes (typically under a minute).
	 * @param Prompt Full image description. Refine the user's idea into a detailed prompt: subject, style, materials, lighting, background.
	 * @param AssetName Name for the imported Texture asset; pass an empty string to derive it from the prompt.
	 * @return JSON with asset_path of the imported Texture and image_file, a PNG on disk the client shows in chat.
	 */
	UFUNCTION(meta = (AICallable), Category = "AscentGeneration")
	static UToolCallAsyncResultString* GenerateImage(const FString& Prompt, const FString& AssetName);

	/**
	 * Generates a textured 3D mesh from a text prompt through the user's selected 3D provider and imports it into the project. Call this only after asking whether the user wants to start from an image, and they declined. Runs asynchronously and can take a few minutes; the result arrives when the mesh is imported. The chat already shows live progress and then a rotatable 3D view of the mesh with Open and Show in Content Browser buttons, so do not repeat the asset path; say it is ready and ask what to change.
	 * @param Prompt Full model description. Refine the user's idea: object, style, materials, level of detail.
	 * @param AssetName Name for the imported mesh asset; pass an empty string to derive it from the prompt.
	 * @return JSON with asset_path of the imported mesh asset.
	 */
	UFUNCTION(meta = (AICallable), Category = "AscentGeneration")
	static UToolCallAsyncResultString* GenerateMeshFromText(const FString& Prompt, const FString& AssetName);

	/**
	 * Generates a textured 3D mesh from an image through the user's selected 3D provider and imports it. Call this after the user chose to start from an image, or already supplied one. Runs asynchronously and can take a few minutes. The chat already shows live progress and then a rotatable 3D view of the mesh with Open and Show in Content Browser buttons, so do not repeat the asset path; say it is ready and ask what to change.
	 * @param FilePath Absolute path of a PNG to use as the source, e.g. one you generated with your image skill; pass an empty string to reuse the last GenerateImage result of this session.
	 * @param AssetName Name for the imported mesh asset; pass an empty string to derive it from the source image.
	 * @return JSON with asset_path of the imported mesh asset.
	 */
	UFUNCTION(meta = (AICallable), Category = "AscentGeneration")
	static UToolCallAsyncResultString* GenerateMeshFromImage(const FString& FilePath, const FString& AssetName);

	/**
	 * Imports an image file from disk into the project as a Texture asset. Use it after generating an image with your own image generation skill (Codex) to bring the PNG into the project.
	 * @param FilePath Absolute path of the image file, e.g. the PNG saved in your working folder.
	 * @param DestinationPath Content folder for the imported Texture. Pass an empty string to use Ascent Yarin's Default Content Folder from Project Settings. Do not pass /Game/AIGenerated.
	 * @return JSON with asset_path of the imported Texture and image_file, the source file the client shows in chat.
	 */
	UFUNCTION(meta = (AICallable), Category = "AscentGeneration")
	static FString ImportImageFile(const FString& FilePath, const FString& DestinationPath);
};
