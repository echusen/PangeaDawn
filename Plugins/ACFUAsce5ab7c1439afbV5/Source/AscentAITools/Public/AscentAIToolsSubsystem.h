// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "ThreeDBackend.h"
#include "AscentAIToolsSubsystem.generated.h"

class UTexture2D;
class FACFAIToolsNotification;
class UACFAIToolsSettings;
class USkeleton;

/** Broadcast when an image generation finishes (success + imported texture, or error). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FACFImageGenerated, UTexture2D*, Texture, bool, bSuccess, const FString&, ErrorMessage);
/** Broadcast when a 3D mesh generation finishes (success + imported asset, or error). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FACFMeshGenerated, UObject*, Mesh, bool, bSuccess, const FString&, ErrorMessage);
/** Broadcast on progress/status updates while a task is running. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FACFStatusChanged, const FString&, Status);

// Native counterparts used by the built-in Slate window (Slate cannot bind dynamic delegates).
DECLARE_MULTICAST_DELEGATE_ThreeParams(FACFImageGeneratedNative, UTexture2D*, bool, const FString&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FACFMeshGeneratedNative, UObject*, bool, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FACFStatusChangedNative, const FText&);

/**
 * Editor subsystem that exposes the whole AI generation pipeline to Blueprint, so
 * you can build your own Editor Utility Widget on top of it. It handles image
 * generation (image providers), 3D mesh generation, asset import, the bottom-right
 * progress notification, and broadcasts events on completion.
 */
UCLASS()
class ASCENTAITOOLS_API UACFAIToolsSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/** Returns the (mutable) settings object so a custom widget can read/configure all options. */
	UFUNCTION(BlueprintCallable, Category = "Ascent AI Tools")
	static UACFAIToolsSettings* GetAIToolsSettings();

	/**
	 * Generates an image from the prompt using the configured provider, imports it as a Texture,
	 * and broadcasts OnImageGenerated. AssetName is the imported asset name (if empty, derived from the prompt).
	 */
	UFUNCTION(BlueprintCallable, Category = "Ascent AI Tools")
	void GenerateImage(const FString& Prompt, const FString& AssetName);

	/**
	 * Generates a 3D mesh from the text prompt, imports it, and broadcasts OnMeshGenerated.
	 * When bRig is true, Meshy auto-rigs the character; the mesh keeps Meshy's skeleton for you to retarget.
	 * HeightMeters of 0 or less uses the value from Project Settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ascent AI Tools")
	void GenerateMeshFromText(const FString& Prompt, const FString& AssetName, bool bRig = false, float HeightMeters = 0.0f);

	/**
	 * Generates a 3D mesh from the last generated image, imports it, and broadcasts OnMeshGenerated.
	 * When bRig is true, Meshy auto-rigs the character; the mesh keeps Meshy's skeleton for you to retarget.
	 * HeightMeters of 0 or less uses the value from Project Settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ascent AI Tools")
	void GenerateMeshFromImage(const FString& AssetName, bool bRig = false, float HeightMeters = 0.0f);

	/** The last image generated this session, already imported as a Texture (may be null). */
	UFUNCTION(BlueprintPure, Category = "Ascent AI Tools")
	UTexture2D* GetLastGeneratedImage() const { return LastTexture; }

	/** True if an image has been generated this session (usable for image-to-3D). */
	UFUNCTION(BlueprintPure, Category = "Ascent AI Tools")
	bool HasGeneratedImage() const { return LastImageBytes.Num() > 0; }

	/** Raw PNG bytes of the last generated image, empty when none was generated this session. Used by the assistant to preview and re-send the image in chat. */
	const TArray<uint8>& GetLastImagePng() const { return LastImageBytes; }

	/** Loads a PNG file from disk as the pending source for GenerateMeshFromImage, replacing any previously generated image. Returns false when the file cannot be read. */
	UFUNCTION(BlueprintCallable, Category = "Ascent AI Tools")
	bool LoadPendingImageFromFile(const FString& FilePath);

	UFUNCTION(BlueprintPure, Category = "Ascent AI Tools")
	bool IsImageBusy() const { return bImageBusy; }

	UFUNCTION(BlueprintPure, Category = "Ascent AI Tools")
	bool IsMeshBusy() const { return bMeshBusy; }

	/** The most recent progress text of any generation, for example "WaveSpeed: generating 3D mesh (45s)". Lets a view show live detail without subscribing. */
	const FText& GetLastStatus() const { return LastStatus; }

	/** Skeleton assigned to the last successful rigged import (may be null). */
	UFUNCTION(BlueprintPure, Category = "Ascent AI Tools")
	USkeleton* GetLastAssignedSkeleton() const { return LastAssignedSkeleton; }

	UPROPERTY(BlueprintAssignable, Category = "Ascent AI Tools")
	FACFStatusChanged OnStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Ascent AI Tools")
	FACFImageGenerated OnImageGenerated;

	UPROPERTY(BlueprintAssignable, Category = "Ascent AI Tools")
	FACFMeshGenerated OnMeshGenerated;

	// Native delegates for the built-in Slate window.
	FACFStatusChangedNative OnStatusChangedNative;
	FACFImageGeneratedNative OnImageGeneratedNative;
	FACFMeshGeneratedNative OnMeshGeneratedNative;

private:
	void BroadcastStatus(const FText& Status);
	void HandleImageResult(bool bSuccess, const TArray<uint8>& Png, const FString& Error, FString AssetName, TSharedPtr<FACFAIToolsNotification> Notification);
	void HandleMeshResult(bool bSuccess, const FACF3DMeshResult& Mesh, const FString& Error, FString AssetName, const FText& ProviderName, TSharedPtr<FACFAIToolsNotification> Notification);
	/** Starts the backend job for a request already checked by CheckMeshBackend. */
	void StartMeshJob(const IACF3DBackend& Backend, FACF3DRequest Request, bool bFromImage, const FString& ResolvedName);
	/** True when the backend can do this kind of generation and its key is set. Otherwise reports the problem and returns false. */
	bool CheckMeshBackend(const IACF3DBackend& Backend, bool bFromImage);
	void FailMeshStart(const FString& Message);

	static FString ResolveBaseName(const FString& AssetName, const FString& Prompt, const FString& Fallback);

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LastTexture = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USkeleton> LastAssignedSkeleton = nullptr;

	TArray<uint8> LastImageBytes;
	bool bImageBusy = false;
	bool bMeshBusy = false;
	FText LastStatus;
};
