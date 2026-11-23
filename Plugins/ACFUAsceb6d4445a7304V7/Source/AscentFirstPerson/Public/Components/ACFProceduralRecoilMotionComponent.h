// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ACFProceduralMotionComponent.h"
#include "Presets/ACFProceduralPresetRecoilMotion.h"
#include "ACFProceduralRecoilMotionComponent.generated.h"

class ACharacter;

// Runtime state for a single axis recoil
USTRUCT()
struct FACFRecoilAxisState
{
	GENERATED_BODY()

	float Target = 0.0f;
	float Current = 0.0f;
	float Velocity = 0.0f;
	float StartTime = 0.0f;
	bool bActive = false;

	void Reset()
	{
		Target = Current = Velocity = StartTime = 0.0f;
		bActive = false;
	}
};

// Complete recoil state for rotation or location
USTRUCT()
struct FACFRecoilTransformState
{
	GENERATED_BODY()

	FACFRecoilAxisState X;
	FACFRecoilAxisState Y;
	FACFRecoilAxisState Z;

	void Reset()
	{
		X.Reset();
		Y.Reset();
		Z.Reset();
	}
};

UCLASS(ClassGroup=(ACFFirstPerson), meta=(BlueprintSpawnableComponent, DisplayName="Recoil Component"))
class ASCENTFIRSTPERSON_API UACFProceduralRecoilMotionComponent : public UACFProceduralMotionComponent
{
	GENERATED_BODY()

public:
	UACFProceduralRecoilMotionComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UACFProceduralMotionComponent
	virtual TSubclassOf<UACFProceduralPreset> GetPresetClass() const override;
	//~ End of UACFProceduralMotionComponent

	//~ Begin UActorComponent
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

public:
	// Initialize with recoil data and optional fire rate
	UFUNCTION(BlueprintCallable, Category="Recoil")
	void Init(float FireRateRPM = 600.0f);

	// Trigger recoil (call on each shot)
	UFUNCTION(BlueprintCallable, Category="Recoil")
	void Fire();

	// Stop firing (for auto/burst modes)
	UFUNCTION(BlueprintCallable, Category="Recoil")
	void StopFiring();

	// Set fire mode
	UFUNCTION(BlueprintCallable, Category="Recoil")
	void SetFireMode(EACFRecoilFireMode NewMode);
	
	UFUNCTION(BlueprintCallable, Category="Recoil")
	void StartSimulation();
	UFUNCTION(BlueprintCallable, Category="Recoil")
	void StopSimulation();

	// Get the recoil transform to apply to weapon mesh
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Recoil")
	FTransform GetRecoilTransform() const;

	// Get just rotation output
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Recoil")
	FRotator GetRecoilRotation() const { return OutputRotation; }

	// Get just location output
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Recoil")
	FVector GetRecoilLocation() const { return OutputLocation; }

	// Time since last shot
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Recoil")
	float GetTimeSinceLastShot() const;

	// Enable/disable controller recoil (camera push)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil")
	bool bEnableControllerRecoil = true;

protected:
	virtual void ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs) override;

private:
	// Process single axis with overshoot
	float ProcessAxis(FACFRecoilAxisState& State, const FACFOvershootParams& Params, float DeltaTime, float CurrentTime) const;

	// Generate random target values based on current state
	void GenerateRecoilTargets();

	// Apply controller recoil to character
	void TickControllerRecoil(float DeltaTime);

	// Check if recoil animation is complete
	bool IsRecoilComplete() const;

	// Get current overshoot config based on fire mode
	const FACFAdvancedOvershootConfig& GetCurrentOvershootConfig() const;

	// Get current recoil config based on aiming state
	const FACFRotationRecoilConfig& GetRotationConfig() const;
	const FACFLocationRecoilConfig& GetLocationConfig() const;

public:
	/** Recoil pivot while aiming. Filled in by SetAimPointTransform on an aim component on the same actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Recoil")
	FTransform ADSOffset = FTransform::Identity;

	// Used for applying a overall scale to the recoil, affects everything (0.0 means no recoil, 2.0 means double strength)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Recoil")
	float RecoilScale = 1.0f;

private:
	// Current state
	EACFRecoilFireMode FireMode = EACFRecoilFireMode::Semi;
	float AimingState = 0.f;
	bool bIsFiring = false;
	bool bLastRollPositive = false;

	// Fire rate timing
	float FireRate = 600.0f;
	float LastShotTime = 0.0f;
	int32 ConsecutiveShots = 0;

	// Recoil animation state
	FACFRecoilTransformState RotationState;
	FACFRecoilTransformState LocationState;

	// Smoothed output for auto fire
	FRotator SmoothedRotation = FRotator::ZeroRotator;
	FVector SmoothedLocation = FVector::ZeroVector;

	// Final output
	FRotator OutputRotation = FRotator::ZeroRotator;
	FVector OutputLocation = FVector::ZeroVector;

	// Controller recoil state
	FVector2D ControllerRecoilTarget = FVector2D::ZeroVector;
	FVector2D ControllerRecoilCurrent = FVector2D::ZeroVector;

	// Simulation timer
	FTimerHandle SimulationTimerHandle;
	bool bSimulating = false;

	// State variables for compounding recoil climb
	float CurrentPitchProgression = 0.0f;
	float TargetPitchProgression = 0.0f;
	float CurrentZProgression = 0.0f;
	float TargetZProgression = 0.0f;
};
