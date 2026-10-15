// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFProceduralMotionComponent.h"
#include "Curves/CurveVector.h"
#include "GameplayTagContainer.h"
#include "ACFProceduralAdditiveMotionComponent.generated.h"

/** Playback state for one curve-driven montage. */
USTRUCT(BlueprintType)
struct FACFProceduralCurveMontage
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Curve Montage")
	float CurveMontagePlayback = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Curve Montage")
	float CurveMontagePlayRate = 1.f;

	/** Blend from CurveMontageCache into the live curve value. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Curve Montage")
	float CurveMontageBlend = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Curve Montage")
	FVector CurveMontageLocScale = FVector::OneVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Curve Montage")
	FVector CurveMontageRotScale = FVector::OneVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Curve Montage")
	FTransform CurveMontageOut = FTransform::Identity;

	/** Pose at the moment the montage started, blended out of. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Curve Montage")
	FTransform CurveMontageCache = FTransform::Identity;

	UPROPERTY(Transient)
	TObjectPtr<UCurveVector> TargetCurveMontageRot;

	UPROPERTY(Transient)
	TObjectPtr<UCurveVector> TargetCurveMontageLoc;

	float GetLength() const;
	bool IsPlaying() const;
};

/** Additive motion from baked animation curves, plus tag-triggered curve montages. */
UCLASS(ClassGroup=(ACFFirstPerson), meta=(BlueprintSpawnableComponent, DisplayName="Additive Component"))
class ASCENTFIRSTPERSON_API UACFProceduralAdditiveMotionComponent : public UACFProceduralMotionComponent
{
	GENERATED_BODY()

public:
	UACFProceduralAdditiveMotionComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UACFProceduralMotionComponent
	virtual TSubclassOf<UACFProceduralPreset> GetPresetClass() const override;
	//~ End of UACFProceduralMotionComponent

	//~ Begin UActorComponent
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	//~ End UActorComponent

	/** Starts the montage bound to the current tag in project settings. */
	UFUNCTION(BlueprintCallable, Category="Additive")
	virtual void PlayProceduralCurveMontage();

	/** Sets the tag and plays it. Ignored while a non-cancellable montage is running. */
	UFUNCTION(BlueprintCallable, Category="Additive")
	virtual void SetProceduralCurveMontageTag(FGameplayTag InTag);

	UFUNCTION(Server, Reliable)
	void ServerSetProceduralCurveMontageTag(FGameplayTag InTag);

protected:
	//~ Begin UACFProceduralMotionComponent
	virtual void ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs) override;
	//~ End UACFProceduralMotionComponent

	void RefreshProceduralCurveMontage(float DeltaSeconds);

	UFUNCTION()
	virtual void OnRep_ProceduralCurveMontageTag();

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State|Additive")
	FACFProceduralCurveMontage ProceduralCurveMontage;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State|Additive", ReplicatedUsing=OnRep_ProceduralCurveMontageTag)
	FGameplayTag ProceduralCurveMontageTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State|Additive")
	uint8 bIsProceduralCurveMontagePlaying : 1;

	FTimerHandle TimerHandle_ProceduralCurveMontage;
};
