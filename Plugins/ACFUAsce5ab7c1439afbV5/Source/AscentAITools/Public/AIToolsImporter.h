// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;
class UObject;
class USkeletalMesh;
class USkeleton;

/**
 * Helper that turns raw bytes returned by the AI services into real, saved
 * Unreal assets by writing them to a temporary file and running the editor
 * import pipeline (Interchange / glTF importer for meshes, image importer for textures).
 */
class ASCENTAITOOLS_API FACFAIToolsImporter
{
public:
	/**
	 * Imports PNG bytes as a UTexture2D into the Default Content Folder from Project Settings.
	 * @return the imported texture, or nullptr on failure.
	 */
	static UTexture2D* ImportTexturePng(const TArray<uint8>& PngBytes, const FString& BaseName);

	/**
	 * Imports mesh bytes (typically GLB) as a Static Mesh (+ materials/textures) into the Default Content Folder from Project Settings.
	 * @param OutImported   Receives every object created by the import.
	 * @return the primary imported mesh asset, or nullptr on failure.
	 */
	static UObject* ImportMeshGlb(const TArray<uint8>& GlbBytes, const FString& BaseName, TArray<UObject*>& OutImported);

	/**
	 * Imports a Meshy auto-rigged FBX as a Skeletal Mesh, keeping Meshy's own skeleton
	 * (Mixamo-style). No remap onto your own skeleton, retarget in the editor if needed.
	 * @param OutError   Receives a human-readable failure reason when the return is nullptr.
	 * @return the imported USkeletalMesh, or nullptr on failure.
	 */
	static USkeletalMesh* ImportRiggedFbx(const TArray<uint8>& FbxBytes, const FString& BaseName, TArray<UObject*>& OutImported, FString& OutError);
};
