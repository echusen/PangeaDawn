#include "Components/MiningPresentationAgentComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Data/ACFCharacterInitializerComponent.h"
#include "Net/UnrealNetwork.h"

UMiningPresentationAgentComponent::UMiningPresentationAgentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	WorkerWorkLoopAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Yarin/AS_Pangea_MiningStrike.AS_Pangea_MiningStrike")));
	RetrievalAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Yarin/AS_Pangea_MinerRetrieve.AS_Pangea_MinerRetrieve")));
	DepositAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Yarin/AS_Pangea_MinerDeposit.AS_Pangea_MinerDeposit")));
	GuardHoldLoopAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/FullSample/Animations/Locomotion/Unarmed/MM_Unarmed_Idle_Ready.MM_Unarmed_Idle_Ready")));
	CourierInteractionLoopAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/FullSample/Animations/Quaternius/AL_Anim_Rig_PickUp_Table.AL_Anim_Rig_PickUp_Table")));
}

void UMiningPresentationAgentComponent::SetTravelSpeedOverride(const EMiningPresentationRole Role, const float TravelSpeed)
{
	if (!CanAuthorPresentation() || !FMath::IsFinite(TravelSpeed) || TravelSpeed <= 0.0f)
	{
		return;
	}

	switch (Role)
	{
	case EMiningPresentationRole::Worker:
		WorkerTravelSpeed = TravelSpeed;
		break;
	case EMiningPresentationRole::Guard:
		GuardTravelSpeed = TravelSpeed;
		break;
	case EMiningPresentationRole::Courier:
		CourierTravelSpeed = TravelSpeed;
		break;
	default:
		break;
	}

	ApplyMovementForState();
}

void UMiningPresentationAgentComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheOwnerReferences();
	if (UACFCharacterInitializerComponent* Initializer = GetOwner()->FindComponentByClass<UACFCharacterInitializerComponent>())
	{
		Initializer->OnAppearanceChanged.AddUniqueDynamic(this, &ThisClass::HandleAppearanceChanged);
	}
	PublishPresentationSnapshot();
	ApplyMovementForState();
	ApplyAnimationForState();
}

void UMiningPresentationAgentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMiningPresentationAgentComponent, PresentationSnapshot);
}

bool UMiningPresentationAgentComponent::CanAuthorPresentation() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

void UMiningPresentationAgentComponent::PublishPresentationSnapshot()
{
	if (!CanAuthorPresentation()) return;
	PresentationSnapshot.Role = CurrentRole;
	PresentationSnapshot.State = CurrentState;
	PresentationSnapshot.FocusLocation = CurrentFocusLocation;
	PresentationSnapshot.bHasFocus = bHasFocus;
	GetOwner()->ForceNetUpdate();
}

void UMiningPresentationAgentComponent::OnRep_PresentationSnapshot()
{
	// No client RPC, movement request, station reservation or production side effects.
	CurrentRole = PresentationSnapshot.Role;
	CurrentState = PresentationSnapshot.State;
	CurrentFocusLocation = PresentationSnapshot.FocusLocation;
	bHasFocus = PresentationSnapshot.bHasFocus;
	ApplyAnimationForState();
}

void UMiningPresentationAgentComponent::HandleAppearanceChanged(UACFCharacterDataAsset* CharacterData)
{
	// A replicated ACF appearance may arrive after our initial snapshot/BeginPlay.
	ApplyAnimationForState();
}

void UMiningPresentationAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner())
		if (UACFCharacterInitializerComponent* Initializer = GetOwner()->FindComponentByClass<UACFCharacterInitializerComponent>())
			Initializer->OnAppearanceChanged.RemoveDynamic(this, &ThisClass::HandleAppearanceChanged);
	StopPresentationAnimation();
	CurrentState = EMiningPresentationState::Idle;
	ApplyMovementForState();
	Super::EndPlay(EndPlayReason);
}

void UMiningPresentationAgentComponent::SetMiningPresentationRole_Implementation(EMiningPresentationRole Role)
{
	if (!CanAuthorPresentation() || CurrentRole == Role)
	{
		return;
	}

	CurrentRole = Role;
	PublishPresentationSnapshot();
	ApplyMovementForState();
	ApplyAnimationForState();
}

void UMiningPresentationAgentComponent::SetMiningPresentationState_Implementation(EMiningPresentationState State)
{
	if (!CanAuthorPresentation() || CurrentState == State)
	{
		return;
	}

	CurrentState = State;
	PublishPresentationSnapshot();
	ApplyMovementForState();
	ApplyAnimationForState();
}

void UMiningPresentationAgentComponent::SetMiningPresentationFocus_Implementation(const FVector& WorldLocation)
{
	if (!CanAuthorPresentation() || WorldLocation.ContainsNaN()) return;
	if (bHasFocus && CurrentFocusLocation.Equals(WorldLocation, 1.0f))
	{
		return;
	}

	CurrentFocusLocation = WorldLocation;
	bHasFocus = true;
	PublishPresentationSnapshot();
}

void UMiningPresentationAgentComponent::CacheOwnerReferences()
{
	ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner());
	CachedCharacterOwner = CharacterOwner;
	CachedMovementComponent = CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr;
	UAnimInstance* CurrentAnim = (CharacterOwner && CharacterOwner->GetMesh()) ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr;
	if (CachedAnimInstance.Get() != CurrentAnim)
	{
		StopPresentationAnimation(); // Restore only the old instance's owned montage/root-motion override.
		ActivePresentationMontage = nullptr;
		bHasSavedRootMotionMode = false;
		CachedAnimInstance = CurrentAnim;
	}
	if (CachedMovementComponent.IsValid() && DefaultMaxWalkSpeed <= 0.0f)
	{
		DefaultMaxWalkSpeed = CachedMovementComponent->MaxWalkSpeed;
	}
}

void UMiningPresentationAgentComponent::ApplyMovementForState()
{
	if (!CanAuthorPresentation()) return; // Simulated proxies use CharacterMovement replication, never local duty steering.
	CacheOwnerReferences();
	if (!CachedMovementComponent.IsValid())
	{
		return;
	}

	if (CurrentState == EMiningPresentationState::Traveling)
	{
		if (!bOwnsTravelSpeed) DefaultMaxWalkSpeed = CachedMovementComponent->MaxWalkSpeed;
		AppliedTravelSpeed = GetTravelSpeedForRole();
		CachedMovementComponent->MaxWalkSpeed = AppliedTravelSpeed;
		bOwnsTravelSpeed = true;
		return;
	}

	// ACF may already have applied combat/command locomotion before the routine
	// yields. Restore only our own override, not a newer owner's speed.
	if (bOwnsTravelSpeed && FMath::IsNearlyEqual(CachedMovementComponent->MaxWalkSpeed, AppliedTravelSpeed) && DefaultMaxWalkSpeed > 0.0f)
	{
		CachedMovementComponent->MaxWalkSpeed = DefaultMaxWalkSpeed;
	}
	bOwnsTravelSpeed = false;
}

void UMiningPresentationAgentComponent::ApplyAnimationForState()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	CacheOwnerReferences();
	if (!CachedAnimInstance.IsValid())
	{
		return;
	}

	float Rate;
	int32 Loops;
	TSoftObjectPtr<UAnimSequenceBase> Clip = SelectPresentationAnimation(CurrentState, Rate, Loops);
	if (!Clip.IsNull() && PlayLoopingPresentationAnimation(Clip, Rate, Loops)) return;
	StopPresentationAnimation();
}

TSoftObjectPtr<UAnimSequenceBase> UMiningPresentationAgentComponent::SelectPresentationAnimation(
	EMiningPresentationState State, float& OutRate, int32& OutLoopCount) const
{
	OutRate = 1.f;
	OutLoopCount = 1;
	if (CurrentRole == EMiningPresentationRole::Worker && State == EMiningPresentationState::Working)
	{
		OutRate = WorkerWorkLoopPlayRate;
		OutLoopCount = WorkerWorkLoopCount;
		return WorkerWorkLoopAnimation;
	}
	if (CurrentRole == EMiningPresentationRole::Worker && State == EMiningPresentationState::Retrieving) return RetrievalAnimation;
	if (CurrentRole == EMiningPresentationRole::Worker && State == EMiningPresentationState::Depositing) return DepositAnimation;
	if (CurrentRole == EMiningPresentationRole::Guard && State == EMiningPresentationState::Guarding)
	{
		OutRate = GuardHoldLoopPlayRate;
		OutLoopCount = GuardHoldLoopCount;
		return GuardHoldLoopAnimation;
	}
	if (CurrentRole == EMiningPresentationRole::Courier &&
		(State == EMiningPresentationState::Loading || State == EMiningPresentationState::Unloading))
	{
		const TSoftObjectPtr<UAnimSequenceBase> Interaction = State == EMiningPresentationState::Loading ? RetrievalAnimation : DepositAnimation;
		if (!Interaction.IsNull()) return Interaction;
		OutRate = CourierInteractionLoopPlayRate;
		OutLoopCount = CourierInteractionLoopCount;
		return CourierInteractionLoopAnimation; // Legacy optional fallback.
	}
	return {};
}

float UMiningPresentationAgentComponent::GetMiningPresentationDuration_Implementation(EMiningPresentationState State)
{
	float Rate;
	int32 Loops;
	TSoftObjectPtr<UAnimSequenceBase> Clip = SelectPresentationAnimation(State, Rate, Loops);
	const UAnimSequenceBase* Animation = Clip.IsNull() ? nullptr : Clip.LoadSynchronous();
	const float EffectiveRate = Animation ? FMath::Abs(Animation->RateScale * Rate) : 0.f;
	return Animation && FMath::IsFinite(EffectiveRate) && EffectiveRate > UE_SMALL_NUMBER
		? Animation->GetPlayLength() / EffectiveRate : 0.f;
}

bool UMiningPresentationAgentComponent::PlayLoopingPresentationAnimation(TSoftObjectPtr<UAnimSequenceBase>& AnimationAsset, float PlayRate, int32 LoopCount)
{
	if (!CachedAnimInstance.IsValid())
	{
		return false;
	}

	UAnimSequenceBase* Animation = AnimationAsset.LoadSynchronous();
	if (!Animation)
	{
		return false;
	}

	if (ActivePresentationMontage && ActivePresentationAnimation.Get() == Animation && CachedAnimInstance->Montage_IsPlaying(ActivePresentationMontage))
	{
		return true;
	}
	if (LoopCount == 1 && ActivePresentationMontage && ActivePresentationAnimation.Get() == Animation &&
		ActiveAnimationState == CurrentState && ActiveAnimationRole == CurrentRole)
		return true; // Focus-only replication must not replay a completed one-shot.

	StopPresentationAnimation();
	if (!bHasSavedRootMotionMode)
	{
		SavedRootMotionMode = CachedAnimInstance->RootMotionMode;
		bHasSavedRootMotionMode = true;
	}

	CachedAnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	ActivePresentationMontage = CachedAnimInstance->PlaySlotAnimationAsDynamicMontage(
		Animation,
		PresentationMontageSlotName,
		PresentationMontageBlendInTime,
		PresentationMontageBlendOutTime,
		PlayRate,
		LoopCount,
		-1.0f,
		0.0f);
	ActivePresentationAnimation = Animation;
	ActiveAnimationState = CurrentState;
	ActiveAnimationRole = CurrentRole;
	return ActivePresentationMontage != nullptr;
}

void UMiningPresentationAgentComponent::StopPresentationAnimation()
{
	ActivePresentationAnimation.Reset();
	if (!CachedAnimInstance.IsValid())
	{
		return;
	}

	if (ActivePresentationMontage)
	{
		CachedAnimInstance->Montage_Stop(PresentationMontageBlendOutTime, ActivePresentationMontage);
		ActivePresentationMontage = nullptr;
	}

	if (bHasSavedRootMotionMode)
	{
		if (CachedAnimInstance->RootMotionMode == ERootMotionMode::IgnoreRootMotion) CachedAnimInstance->SetRootMotionMode(SavedRootMotionMode);
		bHasSavedRootMotionMode = false;
	}
}

float UMiningPresentationAgentComponent::GetTravelSpeedForRole() const
{
	switch (CurrentRole)
	{
	case EMiningPresentationRole::Worker:
		return WorkerTravelSpeed;
	case EMiningPresentationRole::Guard:
		return GuardTravelSpeed;
	case EMiningPresentationRole::Courier:
		return CourierTravelSpeed;
	default:
		return DefaultMaxWalkSpeed > 0.0f ? DefaultMaxWalkSpeed : WorkerTravelSpeed;
	}
}
