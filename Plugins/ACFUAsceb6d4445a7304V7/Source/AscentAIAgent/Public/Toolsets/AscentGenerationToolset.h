// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"

#include "AscentGenerationToolset.generated.h"

/// AI asset generation tools backed by the Ascent AI Tools subsystem: image generation, 3D mesh generation (Meshy, Tripo or WaveSpeed, whichever 3D provider the user selected) and sound effect generation, with automatic import into the project.
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
	 * Generates a sound effect from a text prompt with the ElevenLabs sound effects model (SFX v3 by default) and imports it
	 * as a Sound Wave in the Audio subfolder of the Yarin content folder. Runs asynchronously; the result arrives when the
	 * sound is imported (usually seconds). Use it for any one-shot or looping game sound: impacts, footsteps, magic, creature
	 * noises, ambiences, UI blips.
	 * @param Prompt Full sound description. Refine the user's idea into the action, setting, atmosphere and performance wanted
	 * ("heavy iron sword hitting a wooden shield in a stone hall"), not a bare category ("sword hit"); SFX v3 follows a described
	 * scene much better. It also generates musical elements ("90s hip-hop drum loop, 90 BPM").
	 * @param AssetName Name for the imported Sound Wave, without the A_ prefix; pass an empty string to derive it from the prompt.
	 * @param DurationSeconds Length in seconds, clamped to 0.5 - 30. Pass 0 to let the model choose it from the prompt.
	 * @param bLoop Generates a seamless loop, for ambiences and background textures, and marks the Sound Wave as looping.
	 * @return JSON with asset_path of the imported Sound Wave, model, duration_seconds and loop.
	 */
	UFUNCTION(meta = (AICallable, MutatingTool), Category = "ACF")
	static UToolCallAsyncResultString* GenerateSoundEffect(const FString& Prompt, const FString& AssetName, float DurationSeconds, bool bLoop);

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

	/**
	 * Generates a full-body skeletal animation from a text description with Kimodo and imports it already retargeted onto a character.
	 * Use this when the user asks for a motion, walk, attack, gesture, or other body animation. Runs on this computer. It never downloads or installs anything: when Kimodo is not installed it returns an error, and you tell the user to open Yarin settings > Motion (Kimodo), choose a folder and press Download. Do not ask the user to use a terminal.
	 * @param Prompt What the body does, for example "a person draws a sword and steps forward".
	 * @param DurationSeconds Length of the clip in seconds. Pass 0 to use 5 seconds. Clamped to 1-20.
	 * @param AssetName Name for the AnimSequence. Pass an empty string to derive it from the prompt. An animation with the same name is replaced.
	 * @param TargetMesh Skeletal mesh to put the motion on, as an asset name (e.g. "SKM_Manny") or object path. Pass an empty string for SKM_Manny. When the mesh does not exist SKM_Manny is used and "fallback" is true.
	 * @return JSON with asset_path of the retargeted AnimSequence, mesh and mesh_path it was made for, fallback, source_path of the raw SOMA clip, frames, frame_rate, and bvh_file on disk.
	 */
	UFUNCTION(meta = (AICallable, MutatingTool), Category = "ACF")
	static UToolCallAsyncResultString* GenerateMotionFromText(const FString& Prompt, float DurationSeconds, const FString& AssetName, const FString& TargetMesh);
};
