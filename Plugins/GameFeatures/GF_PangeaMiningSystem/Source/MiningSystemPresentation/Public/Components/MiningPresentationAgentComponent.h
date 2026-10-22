#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimEnums.h"
#include "Components/ActorComponent.h"
#include "Interfaces/MiningPresentationAgentInterface.h"
#include "MiningPresentationAgentComponent.generated.h"

class ACharacter;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UCharacterMovementComponent;
class UACFCharacterDataAsset;

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

UCLASS(ClassGroup=(Pangea), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class MININGSYSTEMPRESENTATION_API UMiningPresentationAgentComponent : public UActorComponent, public IMiningPresentationAgentInterface
{
	GENERATED_BODY()

public:
	UMiningPresentationAgentComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining|Presentation")
	void SetTravelSpeedOverride(EMiningPresentationRole Role, float TravelSpeed);

	UFUNCTION(BlueprintPure, Category="Mining|Presentation")
	EMiningPresentationRole GetCurrentRole() const { return CurrentRole; }

	UFUNCTION(BlueprintPure, Category="Mining|Presentation")
	EMiningPresentationState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category="Mining|Presentation")
	bool HasPresentationFocus() const { return bHasFocus; }

	UFUNCTION(BlueprintPure, Category="Mining|Presentation")
	FVector GetPresentationFocusLocation() const { return CurrentFocusLocation; }

	UFUNCTION(BlueprintPure, Category="Mining|Presentation|Diagnostics")
	UAnimSequenceBase* GetActivePresentationAnimation() const { return ActivePresentationAnimation.Get(); }

	virtual void SetMiningPresentationRole_Implementation(EMiningPresentationRole Role) override;
	virtual void SetMiningTravelSpeed_Implementation(EMiningPresentationRole Role, float Speed) override { SetTravelSpeedOverride(Role, Speed); }
	virtual void SetMiningPresentationState_Implementation(EMiningPresentationState State) override;
	virtual void SetMiningPresentationFocus_Implementation(const FVector& WorldLocation) override;
	virtual float GetMiningPresentationDuration_Implementation(EMiningPresentationState State) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	TSoftObjectPtr<UAnimSequenceBase> WorkerWorkLoopAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	TSoftObjectPtr<UAnimSequenceBase> RetrievalAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	TSoftObjectPtr<UAnimSequenceBase> DepositAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	TSoftObjectPtr<UAnimSequenceBase> GuardHoldLoopAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	TSoftObjectPtr<UAnimSequenceBase> CourierInteractionLoopAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	FName PresentationMontageSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	float PresentationMontageBlendInTime = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	float PresentationMontageBlendOutTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	float WorkerWorkLoopPlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	float GuardHoldLoopPlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	float CourierInteractionLoopPlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	int32 WorkerWorkLoopCount = 9999;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	int32 GuardHoldLoopCount = 9999;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation|Animation")
	int32 CourierInteractionLoopCount = 9999;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation")
	float WorkerTravelSpeed = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation")
	float GuardTravelSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Presentation")
	float CourierTravelSpeed = 320.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Presentation")
	EMiningPresentationRole CurrentRole = EMiningPresentationRole::Worker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Presentation")
	EMiningPresentationState CurrentState = EMiningPresentationState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Presentation")
	FVector CurrentFocusLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Presentation")
	bool bHasFocus = false;

private:
	UPROPERTY(ReplicatedUsing=OnRep_PresentationSnapshot, Transient)
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
