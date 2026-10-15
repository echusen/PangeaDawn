// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Scene.h"
#include "ACFCharacterPreviewStage.generated.h"

class UArrowComponent;
class UPostProcessComponent;
class USpotLightComponent;
class USphereComponent;

UCLASS(Blueprintable,meta=(DisplayName="UI Character Preview Stage"))
class INVENTORYSYSTEM_API AACFCharacterPreviewStage : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AACFCharacterPreviewStage();

	UFUNCTION(BlueprintCallable, Category="ACF|Preview")
	TArray<USceneComponent*> GetPreviewSpawnPoints(){return PreviewSpawnPoints;}

	UFUNCTION(BlueprintCallable, Category="ACF|Preview")
	void PositioningCharactersOnPreviewSphere(TArray<ACharacter*>& Characters,const float Radius,const float PreviewSpacingMultiplier);

	UFUNCTION(BlueprintCallable, Category="ACF|Preview")
	void MoveCharactersOnPreviewSphere(TArray<ACharacter*>& Characters);

	/** Updates the post process exposure settings at runtime */
	UFUNCTION(BlueprintCallable, Category="ACF|Preview|PostProcess")
	void UpdatePostProcessSettings();

	/** Updates all character lights settings at runtime */
	UFUNCTION(BlueprintCallable, Category="ACF|Preview|Lighting")
	void UpdateCharacterLightsSettings();

	/** Toggles character lights on/off */
	UFUNCTION(BlueprintCallable, Category="ACF|Preview|Lighting")
	void SetCharacterLightsEnabled(bool bEnabled);
protected:
	/** Root component for the preview stage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ACF|Preview", meta=(AllowPrivateAccess="true"))
	USceneComponent* RootSceneComponent;

	/** Spherical room (inside-visible/outside-invisible material applied in editor) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ACF|Preview", meta=(AllowPrivateAccess="true"))
	UStaticMeshComponent* PreviewSphereMesh;

	/** Spawn point for the preview character */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ACF|Preview", meta=(AllowPrivateAccess="true"))
	USceneComponent* PreviewSpawnPoint;

	/** Sphere component that defines the post process volume boundaries */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ACF|Preview|PostProcess", meta=(AllowPrivateAccess="true"))
	USphereComponent* PostProcessVolume;

	/** Post process component for preview stage with exposure settings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ACF|Preview|PostProcess", meta=(AllowPrivateAccess="true"))
	UPostProcessComponent* PostProcessComponent;

	UPROPERTY(Transient)
	TArray<USceneComponent*> PreviewSpawnPoints;

	/** Spot lights attached to each character */
	UPROPERTY(Transient)
	TArray<USpotLightComponent*> CharacterSpotLights;

public:
	/** Enable post process exposure control */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|Preview|PostProcess")
	bool bEnablePostProcess = false;

	/** Auto exposure method (Histogram = adaptive/smooth, Basic = fast/simple, Manual = fixed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|Preview|PostProcess", meta=(EditCondition="bEnablePostProcess"))
	TEnumAsByte<EAutoExposureMethod> ExposureMethod = EAutoExposureMethod::AEM_Histogram;

	/** Exposure compensation (EV) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|Preview|PostProcess", meta=(EditCondition="bEnablePostProcess", ClampMin="-15.0", ClampMax="15.0"))
	float ExposureCompensation = 0.0f;

	/** Min brightness for auto exposure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|Preview|PostProcess", meta=(EditCondition="bEnablePostProcess", ClampMin="0.0", ClampMax="10.0"))
	float MinBrightness = 0.5f;

	/** Max brightness for auto exposure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|Preview|PostProcess", meta=(EditCondition="bEnablePostProcess", ClampMin="0.0", ClampMax="10.0"))
	float MaxBrightness = 2.0f;

	/** Enable spot lights on characters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|Preview|Lighting")
	bool bEnableCharacterLights = true;

	/** Intensity of the spot lights */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|Preview|Lighting", meta=(EditCondition="bEnableCharacterLights", ClampMin="0.0"))
	float CharacterLightIntensity = 10000.0f;

	/** Source radius (length of cone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|Preview|Lighting", meta=(EditCondition="bEnableCharacterLights", ClampMin="0.0"))
	float CharacterLightSourceRadius = 300.0f;

	/** Offset from character location for the spot light */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|Preview|Lighting", meta=(EditCondition="bEnableCharacterLights"))
	FVector CharacterLightOffset = FVector(0.0f, 0.0f, 200.0f);

	/** Rotation offset for the spot light (default points down at character) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|Preview|Lighting", meta=(EditCondition="bEnableCharacterLights"))
	FRotator CharacterLightRotation = FRotator(-90.0f, 0.0f, 0.0f);

protected:


	UFUNCTION(BlueprintCallable, Category="ACF|Preview")
	void GeneratePoints(const int32 NumPoints,const float Radius,const float PreviewSpacingMultiplier);

	UPROPERTY(Transient)
	TMap<ACharacter*,FTransform> CharacterPreviewTransforms;

};
