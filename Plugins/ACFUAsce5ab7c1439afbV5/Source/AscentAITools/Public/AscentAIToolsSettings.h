// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AscentAIToolsSettings.generated.h"

/** Visual style requested from Meshy for the generated mesh. */
UENUM()
enum class EACFMeshyArtStyle : uint8
{
	Realistic	UMETA(DisplayName = "Realistic"),
	Sculpture	UMETA(DisplayName = "Sculpture")
};

/** Mesh topology requested from Meshy. */
UENUM()
enum class EACFMeshyTopology : uint8
{
	Triangle	UMETA(DisplayName = "Triangle"),
	Quad		UMETA(DisplayName = "Quad")
};

/** Base-color texture resolution requested from Meshy (image-to-3D and text-to-3D refine). */
UENUM()
enum class EACFMeshyTextureResolution : uint8
{
	TwoK	UMETA(DisplayName = "2K"),
	FourK	UMETA(DisplayName = "4K"),
	EightK	UMETA(DisplayName = "8K")
};

/** Selectable image generation backend. */
UENUM()
enum class EACFImageProvider : uint8
{
	Gemini		UMETA(DisplayName = "Gemini"),
	GptImage	UMETA(DisplayName = "GPT Image")
};

/** Selectable 3D mesh generation backend. Add new providers at the end: the numeric value is saved in config. */
UENUM()
enum class EACF3DProvider : uint8
{
	Meshy		UMETA(DisplayName = "Meshy"),
	Tripo		UMETA(DisplayName = "Tripo"),
	WaveSpeed	UMETA(DisplayName = "WaveSpeed")
};

/**
 * Project Settings for the Ascent AI Tools: API keys and import options used for
 * AI image generation and AI 3D mesh generation.
 * Stored in DefaultGame.ini so they live alongside the project settings.
 */
UCLASS(BlueprintType, config = Plugins, DefaultConfig, meta = (DisplayName = "Ascent AI Tools"))
class ASCENTAITOOLS_API UACFAIToolsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UACFAIToolsSettings();

	/** Active image generation backend (also selectable from the AI Tools window). */
	UPROPERTY(BlueprintReadWrite, config, Category = "Image Generation")
	EACFImageProvider ImageProvider;

	/** API key of the selected Image Provider. Paste the key from the provider's account page after choosing the provider. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Image Generation", meta = (DisplayName = "Image API Key", PasswordField = true))
	FString ImageApiKey;

	/** Gemini image model identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Image Generation", meta = (DisplayName = "Gemini Model"))
	FString NanoBananaModel;

	/** GPT Image model identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Image Generation", meta = (DisplayName = "GPT Image Model"))
	FString OpenAIModel;

	/** Active 3D mesh generation backend. Each provider keeps its own API key below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "3D Generation", meta = (DisplayName = "3D Provider"))
	EACF3DProvider ThreeDProvider = EACF3DProvider::Meshy;

	/** Meshy API key. Kept under its original config name so saved projects keep working. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "3D Generation", meta = (DisplayName = "Meshy API Key", PasswordField = true))
	FString ThreeDApiKey;

	/** Tripo API key. Create it on your Tripo platform account page. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "3D Generation", meta = (DisplayName = "Tripo API Key", PasswordField = true))
	FString TripoApiKey;

	/** WaveSpeed API key. Create it on your WaveSpeed account page. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "3D Generation", meta = (DisplayName = "WaveSpeed API Key", PasswordField = true))
	FString WaveSpeedApiKey;

	/** API key of the given 3D provider. */
	FString GetThreeDApiKey(EACF3DProvider Provider) const;

	/** API key of the active 3D provider. */
	FString GetActiveThreeDApiKey() const { return GetThreeDApiKey(ThreeDProvider); }

	/** Stores the key for the active 3D provider. */
	void SetActiveThreeDApiKey(const FString& Key);

	// The following 3D generation options are edited from the AI Tools window
	// (not from Project Settings) but are still persisted to config between sessions.
	// The "Meshy" prefix is historical: textures, PBR, target polycount, rigging, character height and
	// poll interval apply to every provider. Art style, topology, texture resolution and auto refine are Meshy only.

	/** Generate textured materials on the mesh (color + PBR maps). Disable for an untextured grey mesh. */
	UPROPERTY(BlueprintReadWrite, config, Category = "3D Generation")
	bool bMeshyGenerateTextures;

	/** Generate PBR maps (metallic/roughness/normal) in addition to the base color. */
	UPROPERTY(BlueprintReadWrite, config, Category = "3D Generation")
	bool bMeshyEnablePBR;

	/** Visual style of the generated mesh. */
	UPROPERTY(BlueprintReadWrite, config, Category = "3D Generation")
	EACFMeshyArtStyle MeshyArtStyle;

	/** Topology of the generated mesh. */
	UPROPERTY(BlueprintReadWrite, config, Category = "3D Generation")
	EACFMeshyTopology MeshyTopology;

	/** Base color texture resolution (Meshy default is 2K; we default to 4K). */
	UPROPERTY(BlueprintReadWrite, config, Category = "3D Generation", meta = (DisplayName = "Texture Resolution"))
	EACFMeshyTextureResolution MeshyTextureResolution;

	/** Target triangle/polygon count for the generated mesh. */
	UPROPERTY(BlueprintReadWrite, config, Category = "3D Generation")
	int32 MeshyTargetPolycount;

	/** Whether to run a refine pass after the preview mesh (slower, higher quality, more credits). */
	UPROPERTY(BlueprintReadWrite, config, Category = "3D Generation")
	bool bMeshyAutoRefine;

	/** When enabled, humanoid generations are auto-rigged by Meshy and imported as a Skeletal Mesh with Meshy's own skeleton (retarget it onto your own skeleton if needed). */
	UPROPERTY(BlueprintReadWrite, config, Category = "3D Generation")
	bool bMeshyRigHumanoid;

	/** Approximate character height in meters passed to Meshy auto-rigging (improves scale/skinning). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "3D Generation", meta = (DisplayName = "Character Height (m)", ClampMin = "0.1", ClampMax = "5.0"))
	float MeshyCharacterHeightMeters;

	/** How often (seconds) we poll the 3D service for task progress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "3D Generation", meta = (DisplayName = "Poll Interval (s)", ClampMin = "1.0", ClampMax = "30.0"))
	float MeshyPollInterval;

	/**
	 * Content folder for generated images and meshes.
	 * Reads Ascent Yarin > Default Content Folder. The setting's own default is /Game/Yarin.
	 */
	static FString GetGeneratedContentFolder();

	/** Lets Ascent Yarin supply its Default Content Folder without an import cycle. Pass nullptr to clear. */
	static void SetGeneratedContentFolderResolver(FString (*Resolver)());

	// ---- Advanced (technical) settings: endpoints, timeouts, low-level params ----

	/** Output image size requested from the image service (e.g. 1024x1024, 1536x1024, 2048x2048). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Image Size"))
	FString ImageSize;

	/** Max seconds to wait for an image generation request before timing out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Image Request Timeout (s)", ClampMin = "5.0", ClampMax = "1200.0"))
	float ImageRequestTimeout;

	/** Base URL for the Gemini image backend (model id is appended). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Gemini Base URL"))
	FString GeminiBaseUrl;

	/** Full endpoint URL for the GPT Image backend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "GPT Image Endpoint"))
	FString OpenAIImageEndpoint;

	/** Base URL of the Meshy API. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Meshy Base URL"))
	FString MeshyBaseUrl;

	/** Meshy path for the text-to-3D endpoint, appended to the base URL. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Meshy Text-to-3D Path"))
	FString TextTo3DPath;

	/** Meshy path for the image-to-3D endpoint, appended to the base URL. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Meshy Image-to-3D Path"))
	FString ImageTo3DPath;

	/** Meshy path for the humanoid rigging endpoint, appended to the base URL. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Meshy Rigging Path"))
	FString RiggingPath;

	/** Base URL of the Tripo API v3. A Tripo key only works on the host of the region it was created in (openapi.tripo3d.ai or openapi.tripo3d.com). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Tripo Base URL"))
	FString TripoBaseUrl;

	/** Tripo model version used for generation, for example v3.1-20260211. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Tripo Model Version"))
	FString TripoModelVersion;

	/** Base URL of the WaveSpeed API v3. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "WaveSpeed Base URL"))
	FString WaveSpeedBaseUrl;

	/** WaveSpeed model id for image-to-3D. Use a Hunyuan3D v3 image-to-3D model, for example wavespeed-ai/hunyuan3d-v3/image-to-3d. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "WaveSpeed Image-to-3D Model"))
	FString WaveSpeedImageModel;

	/** WaveSpeed model id for text-to-3D. Use a Hunyuan3D v3 text-to-3D model, for example wavespeed-ai/hunyuan3d-v3/text-to-3d. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "WaveSpeed Text-to-3D Model"))
	FString WaveSpeedTextModel;

	/** Max seconds to wait for a 3D task creation / refine request. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "3D Request Timeout (s)", ClampMin = "5.0", ClampMax = "600.0"))
	float MeshRequestTimeout;

	/** Max seconds to wait while downloading the generated 3D model. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "3D Download Timeout (s)", ClampMin = "10.0", ClampMax = "1200.0"))
	float MeshDownloadTimeout;

	/** Output model format requested from the 3D service. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Model Format"))
	FString MeshFormat;

	/** Whether to ask the 3D service to remesh (clean topology). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced", meta = (DisplayName = "Should Remesh"))
	bool bShouldRemesh;

	/** Meshy API string for the configured art style ("realistic" / "sculpture"). */
	FString GetArtStyleString() const;

	/** Meshy API string for the configured topology ("triangle" / "quad"). */
	FString GetTopologyString() const;

	/** Meshy API string for texture resolution ("2k" / "4k" / "8k"). */
	FString GetTextureResolutionString() const;

	static const UACFAIToolsSettings* Get();
};
