// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CineCameraComponent.h"
#include "GameplayTagContainer.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "ACFCameraPointComponent.generated.h"



/**
 * Lightweight camera point marker (transform + metadata only).
 */
UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class CINEMATICCAMERAMANAGER_API UACFCameraPointComponent : public UCineCameraComponent
{
	GENERATED_BODY()

public:
	UACFCameraPointComponent();

	/** GameplayTag used to resolve this point (e.g., Camera.Battle.Attack1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Point")
	FGameplayTag CameraTag;

	/** Desired Field Of View when using this point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Point", meta=(ClampMin="1.0", ClampMax="170.0"))
	float DesiredFOV = 70.0f;

	/** Weight used to pick among multiple points having the same tag (higher wins) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Point", meta=(ClampMin="0.0"))
	float SelectionWeight = 1.0f;

	/** If true, this point provides its own blend settings instead of using the caller defaults */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blend", meta=(InlineEditConditionToggle))
	bool bOverrideBlendSettings = false;

	/** Optional: per-point default blend time */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blend",
			  meta=(EditCondition="bOverrideBlendSettings", ClampMin="0.0"))
	float OverrideBlendTime = 1.0f;

	/** Optional: per-point default blend function */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blend",
			  meta=(EditCondition="bOverrideBlendSettings"))
	TEnumAsByte<EViewTargetBlendFunction> OverrideBlendFunction = VTBlend_Cubic;

#if WITH_EDITORONLY_DATA
	/** Editor-only: draw a small frustum gizmo to preview orientation/FOV */
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bDrawPreviewFrustum = true;

	/** Editor-only: length of the preview frustum (cm) */
	UPROPERTY(EditAnywhere, Category="Debug", meta=(ClampMin="10.0", ClampMax="100000.0"))
	float PreviewFrustumLength = 300.0f;
#endif
};
