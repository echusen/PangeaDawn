// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.


#include "ACFAnimSyncComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"


UACFAnimSyncComponent::UACFAnimSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UACFAnimSyncComponent::BeginPlay()
{
	Super::BeginPlay();
	
	MeshComponent = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (!MeshComponent)
	{
		SetComponentTickEnabled(false);
		return;
	}
	
	if (GetOwner()->HasAuthority())
	{
		MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		CaptureServerState();
	}
	else if (SyncedAnim.Anim)
	{
		ApplyReplicatedState();
	}
}

void UACFAnimSyncComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UACFAnimSyncComponent, SyncedAnim);
}

void UACFAnimSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner()->HasAuthority())
	{
		CaptureServerState();
		return;
	}
	
	if (!SyncedAnim.Anim) return;
	MeshComponent->SetPosition(GetExpectedPosition(), false);
}


void UACFAnimSyncComponent::CaptureServerState()
{
	UAnimSingleNodeInstance* SingleNode = MeshComponent->GetSingleNodeInstance();
	UAnimationAsset* CurrentAnim = SingleNode ? SingleNode->GetAnimationAsset() : nullptr;
	if (!CurrentAnim)
	{
		if (SyncedAnim.Anim)
		{
			SyncedAnim = FACFSyncedAnim();
			GetOwner()->ForceNetUpdate();
		}
		return;
	}
	
	const float CurrentPosition = SingleNode->GetCurrentTime();
	const float CurrentPlayRate = SingleNode->GetPlayRate();
	const bool bCurrentLooping = SingleNode->IsLooping();
	const bool bCurrentPlaying = SingleNode->IsPlaying();
	
	bool bStateChanged  = SyncedAnim.Anim != CurrentAnim || !FMath::IsNearlyEqual(SyncedAnim.PlayRate, CurrentPlayRate)
		|| SyncedAnim.bLooping != bCurrentLooping || SyncedAnim.bPlaying != bCurrentPlaying;
	
	if (!bStateChanged && SyncedAnim.Anim)
	{
		const float Length = CurrentAnim->GetPlayLength();
		float PositionError = FMath::Abs(CurrentPosition - GetExpectedPosition());
		if (bCurrentLooping && Length > 0.f)
		{
			PositionError = FMath::Min(PositionError, Length - PositionError);
		}
		constexpr float PositionTolerance = 0.05f;
		bStateChanged = PositionError > PositionTolerance;
	}
	
	if (!bStateChanged) return;
	
	SyncedAnim.Anim = CurrentAnim;
    SyncedAnim.StateServerTime = GetServerTime();
    SyncedAnim.Position = CurrentPosition;
    SyncedAnim.PlayRate = CurrentPlayRate;
    SyncedAnim.bLooping = bCurrentLooping;
    SyncedAnim.bPlaying = bCurrentPlaying;

    GetOwner()->ForceNetUpdate();
}

void UACFAnimSyncComponent::OnRep_SyncedAnim()
{
	ApplyReplicatedState();
}

void UACFAnimSyncComponent::ApplyReplicatedState()
{
	if (!MeshComponent) return;
	if (!SyncedAnim.Anim)
	{
		MeshComponent->Stop();
		return;
	}
	
	MeshComponent->PlayAnimation(SyncedAnim.Anim, SyncedAnim.bLooping);
	MeshComponent->SetPlayRate(SyncedAnim.PlayRate);
	MeshComponent->SetPosition(GetExpectedPosition(), false);
	
	if (!SyncedAnim.bPlaying) { MeshComponent->Stop(); }
}

float UACFAnimSyncComponent::GetExpectedPosition() const
{
	if (!SyncedAnim.Anim) return 0.f;
	
	const float Length = SyncedAnim.Anim->GetPlayLength();
	if (Length <= 0.f) return 0.f;
	
	float Position = SyncedAnim.Position;
	if (SyncedAnim.bPlaying)
	{
		const float Elapsed = FMath::Max(0.f, GetServerTime() - SyncedAnim.StateServerTime);
		Position += Elapsed * SyncedAnim.PlayRate;
	}
	
	if (SyncedAnim.bLooping)
	{
		Position = FMath::Fmod(Position, Length);
		if (Position < 0.f) { Position += Length; }
		return Position;
	}
	
	return FMath::Clamp(Position, 0.f, Length);
}

float UACFAnimSyncComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0.f;

	const AGameStateBase* GameState = World->GetGameState();

	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}
