#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimEnums.h"
#include "Components/ActorComponent.h"
#include "Interfaces/MiningPresentationAgentInterface.h"
#include "MiningPresentationAgentComponent.generated.h"

class ACharacter;
class UACFCharacterDataAsset;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UCharacterMovementComponent;

/** Persistent cosmetic state, reconstructed for newly relevant clients without replaying AI tasks. */
USTRUCT()
struct FMiningAgentPresentationSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	EMiningPresentationRole Role = EMiningPresentationRole::Worker;

	UPROPERTY()
	EMiningPresentationState State = EMiningPresentationState::Idle;

	UPROPERTY()
	FVector FocusLocation = FVector::ZeroVector;

	UPROPERTY()
	bool bHasFocus = false;
};

/**
 * Cosmetic layer for a mining worker, guard or courier. The authority sets role, state and focus;
 * the component turns them into a travel walk speed and a looping slot animation, and replicates
 * one snapshot so clients can rebuild the same presentation without gameplay side effects.
 */
UCLASS(ClassGroup = (Pangea), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class MININGSYSTEMPRESENTATION_API UMiningPresentationAgentComponent : public UActorComponent, public IMiningPresentationAgentInterface
{
	GENERATED_BODY()

public:
	UMiningPresentationAgentComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Sets the travel walk speed for one role. Ignored without authority or when the speed is not finite and positive. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mining|Presentation")
	void SetTravelSpeedOverride(EMiningPresentationRole Role, float TravelSpeed);

	/** Returns the mining role this agent currently presents. */
	UFUNCTION(BlueprintPure, Category = "Mining|Presentation")
	EMiningPresentationRole GetCurrentRole() const
	{
		return CurrentRole;
	}

	/** Returns the presentation state this agent is currently in. */
	UFUNCTION(BlueprintPure, Category = "Mining|Presentation")
	EMiningPresentationState GetCurrentState() const
	{
		return CurrentState;
	}

	/** True once a focus location has been set. */
	UFUNCTION(BlueprintPure, Category = "Mining|Presentation")
	bool HasPresentationFocus() const
	{
		return bHasFocus;
	}

	/** Returns the last focus location. Only meaningful when HasPresentationFocus is true. */
	UFUNCTION(BlueprintPure, Category = "Mining|Presentation")
	FVector GetPresentationFocusLocation() const
	{
		return CurrentFocusLocation;
	}

	/** Returns the animation last started by this component, or null when none is active. */
	UFUNCTION(BlueprintPure, Category = "Mining|Presentation|Diagnostics")
	UAnimSequenceBase* GetActivePresentationAnimation() const
	{
		return ActivePresentationAnimation.Get();
	}

	virtual void SetMiningPresentationRole_Implementation(EMiningPresentationRole Role) override;
	virtual void SetMiningTravelSpeed_Implementation(EMiningPresentationRole Role, float Speed) override
	{
		SetTravelSpeedOverride(Role, Speed);
	}

	virtual void SetMiningPresentationState_Implementation(EMiningPresentationState State) override;
	virtual void SetMiningPresentationFocus_Implementation(const FVector& WorldLocation) override;
	virtual float GetMiningPresentationDuration_Implementation(EMiningPresentationState State) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Looping animation a worker plays while Working. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	TSoftObjectPtr<UAnimSequenceBase> WorkerWorkLoopAnimation;

	/** One-shot animation a worker plays while Retrieving and a courier plays while Loading. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	TSoftObjectPtr<UAnimSequenceBase> RetrievalAnimation;

	/** One-shot animation a worker plays while Depositing and a courier plays while Unloading. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	TSoftObjectPtr<UAnimSequenceBase> DepositAnimation;

	/** Looping animation a guard plays while Guarding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	TSoftObjectPtr<UAnimSequenceBase> GuardHoldLoopAnimation;

	/** Fallback courier Loading / Unloading animation, used only when the retrieval or deposit animation is not set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	TSoftObjectPtr<UAnimSequenceBase> CourierInteractionLoopAnimation;

	/** Anim slot the presentation animation is played in as a dynamic montage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	FName PresentationMontageSlotName = TEXT("DefaultSlot");

	/** Blend-in time of the presentation montage, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	float PresentationMontageBlendInTime = 0.15f;

	/** Blend-out time of the presentation montage, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	float PresentationMontageBlendOutTime = 0.2f;

	/** Play rate of WorkerWorkLoopAnimation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	float WorkerWorkLoopPlayRate = 1.0f;

	/** Play rate of GuardHoldLoopAnimation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	float GuardHoldLoopPlayRate = 1.0f;

	/** Play rate of CourierInteractionLoopAnimation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	float CourierInteractionLoopPlayRate = 1.0f;

	/** How many times WorkerWorkLoopAnimation loops before the montage ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	int32 WorkerWorkLoopCount = 9999;

	/** How many times GuardHoldLoopAnimation loops before the montage ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	int32 GuardHoldLoopCount = 9999;

	/** How many times CourierInteractionLoopAnimation loops before the montage ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation|Animation")
	int32 CourierInteractionLoopCount = 9999;

	/** Max walk speed applied to a worker while Traveling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	float WorkerTravelSpeed = 220.0f;

	/** Max walk speed applied to a guard while Traveling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	float GuardTravelSpeed = 250.0f;

	/** Max walk speed applied to a courier while Traveling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	float CourierTravelSpeed = 320.0f;

	/** Mining role currently presented. Set on the authority and rebuilt on clients from the snapshot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	EMiningPresentationRole CurrentRole = EMiningPresentationRole::Worker;

	/** Presentation state currently shown. Set on the authority and rebuilt on clients from the snapshot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	EMiningPresentationState CurrentState = EMiningPresentationState::Idle;

	/** World location the agent is focused on. Only meaningful when bHasFocus is true. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	FVector CurrentFocusLocation = FVector::ZeroVector;

	/** True once a focus location has been set. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	bool bHasFocus = false;

private:
	/** Replicated copy of role, state and focus that clients apply in OnRep_PresentationSnapshot. */
	UPROPERTY(ReplicatedUsing = OnRep_PresentationSnapshot, Transient)
	FMiningAgentPresentationSnapshot PresentationSnapshot;

	UFUNCTION()
	void OnRep_PresentationSnapshot();

	UFUNCTION()
	void HandleAppearanceChanged(UACFCharacterDataAsset* CharacterData);

	void PublishPresentationSnapshot();
	bool CanAuthorPresentation() const;

	TWeakObjectPtr<ACharacter> CachedCharacterOwner;
	TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActivePresentationMontage;
	TWeakObjectPtr<UAnimSequenceBase> ActivePresentationAnimation;
	EMiningPresentationState ActiveAnimationState = EMiningPresentationState::Idle;
	EMiningPresentationRole ActiveAnimationRole = EMiningPresentationRole::Worker;
	float DefaultMaxWalkSpeed = 0.0f;
	float AppliedTravelSpeed = 0.0f;
	bool bOwnsTravelSpeed = false;
	TEnumAsByte<ERootMotionMode::Type> SavedRootMotionMode;
	bool bHasSavedRootMotionMode = false;

	void CacheOwnerReferences();
	void ApplyMovementForState();
	void ApplyAnimationForState();
	TSoftObjectPtr<UAnimSequenceBase> SelectPresentationAnimation(EMiningPresentationState State, float& OutRate, int32& OutLoopCount) const;
	bool PlayLoopingPresentationAnimation(TSoftObjectPtr<UAnimSequenceBase>& AnimationAsset, float PlayRate, int32 LoopCount);
	void StopPresentationAnimation();
	float GetTravelSpeedForRole() const;
};
