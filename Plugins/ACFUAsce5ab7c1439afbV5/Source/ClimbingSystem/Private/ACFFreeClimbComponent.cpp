// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbComponent.h"

#include "ClimbingSystem.h"
#include "ACFFreeClimbAnimInstance.h"
#include "ACFFreeClimbableComponent.h"
#include "ACFFreeClimbableWorldSubsystem.h"
#include "ACFFreeClimbNetworkRuntime.h"
#include "ACFFreeClimbRigProfile.h"
#include "States/ACFFreeClimbState_Idle.h"
#include "States/ACFFreeClimbState_Entering.h"
#include "States/ACFFreeClimbState_Climb.h"
#include "States/ACFFreeClimbState_Jump.h"
#include "States/ACFFreeClimbState_Cling.h"
#include "States/ACFFreeClimbState_ClimbUp.h"
#include "States/ACFFreeClimbState_ClimbExit.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/GameStateBase.h"
#include "Math/RotationMatrix.h"
#include "Net/UnrealNetwork.h"

namespace
{
    FCollisionObjectQueryParams MakeClimbObjectQuery(const TArray<TEnumAsByte<ECollisionChannel>>& CollisionChannels)
    {
        FCollisionObjectQueryParams ObjectParams;
        for (const TEnumAsByte<ECollisionChannel>& Channel : CollisionChannels)
        {
            if (ObjectParams.IsValidObjectQuery(Channel))
            {
                ObjectParams.AddObjectTypesToQuery(Channel);
            }
        }
        return ObjectParams;
    }

    void FillStaticHitFromTrace(const FHitResult& Hit, FACFSurfaceHitResult& OutHit)
    {
        OutHit.Source = EACFSurfaceSource::Static;
        OutHit.Position = Hit.ImpactPoint;
        OutHit.Normal = Hit.ImpactNormal;
        OutHit.Distance = Hit.Distance;
        OutHit.HitComponent = Hit.GetComponent();
        OutHit.Climbable = nullptr;
    }
}

static bool IsValidClimbState(const EACFFreeClimbState State)
{
    switch (State)
    {
    case EACFFreeClimbState::Idle:
    case EACFFreeClimbState::Entering:
    case EACFFreeClimbState::Climb:
    case EACFFreeClimbState::Jump:
    case EACFFreeClimbState::Cling:
    case EACFFreeClimbState::ClimbUp:
    case EACFFreeClimbState::ClimbExit:
        return true;

    default:
        return false;
    }
}

UACFFreeClimbComponent::UACFFreeClimbComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    
    SetIsReplicatedByDefault(true);
    // Object type "Climbable" from DefaultEngine.ini — not Visibility.
    CollisionChannels.Add(ECC_GameTraceChannel3);
}

UACFFreeClimbComponent::~UACFFreeClimbComponent() = default;

FACFFreeClimbAnimData UACFFreeClimbComponent::GetAnimData() const
{
    const EACFFreeClimbState AnimState  = GetClimbState();
    const FVector2D AnimInput  = UsesRemoteState() ? RemoteVisualMoveInput : ClimbInput;
    
    FACFFreeClimbAnimData Data;
    Data.ClimbState = AnimState;
    Data.ClimbInput = AnimInput;
    Data.ClimbInputSize = AnimInput.Size();

    Data.SurfaceNormal = CurrentSurface.SmoothedNormal;
    Data.SurfacePoint = CurrentSurface.Point;

    Data.ClimbAnimPlayRate = 1.0f;
    Data.JumpProgress = JumpProgress;

    return Data;
}

bool UACFFreeClimbComponent::IsClimbing() const
{
    const EACFFreeClimbState State = GetClimbState();
    return State == EACFFreeClimbState::Entering ||
        State == EACFFreeClimbState::Climb ||
        State == EACFFreeClimbState::Jump ||
        State == EACFFreeClimbState::Cling;
}

bool UACFFreeClimbComponent::GetLimbContact(EACFFreeClimbLimb Limb, FACFFreeClimbContact& OutContact) const
{
    OutContact.Reset();

    if (!LimbContacts.IsValidIndex(GetLimbIndex(Limb)))
    {
        return false;
    }

    const int32 LimbIndex = GetLimbIndex(Limb);

    const FACFFreeClimbContact& Contact = LimbContacts[LimbIndex];

    if (!Contact.bValid)
    {
        return false;
    }

    OutContact = Contact;
    return true;
}

void UACFFreeClimbComponent::BeginPlay()
{
	Super::BeginPlay();

    if (!CacheOwnerComponents())
    {
        SetComponentTickEnabled(false);
        return;
    }
    
    MeshComponent->AddTickPrerequisiteComponent(this);

    if (WalkEnterAngle < MaxClimbAngle)
    {
        UE_LOG(LogACFFreeClimb, Warning,
            TEXT("WalkEnterAngle(%.1f) is below MaxClimbAngle(%.1f). Surfaces in between are grabbable but immediately exit to walking."),
            WalkEnterAngle, MaxClimbAngle);
    }

    if (RigProfile)
    {
        FString ValidationReason;
        bRigProfileValid = RigProfile->IsCompatibleWith(MeshComponent->GetSkeletalMeshAsset(), &ValidationReason);
        if (!bRigProfileValid)
        {
            UE_LOG(LogACFFreeClimb, Warning,
                TEXT("free climb rig profile '%s' is not compatible with mesh '%s'. Limb contact probing and climbing IK will be disabled: %s"),
                *GetNameSafe(RigProfile), *GetNameSafe(MeshComponent->GetSkeletalMeshAsset()), *ValidationReason);
        }
    }
    else
    {
        UE_LOG(LogACFFreeClimb, Warning,
            TEXT("ACFFreeClimbRigProfile is not assigned. Limb contact probing and climbing IK will be disabled."));
    }

    PhysicalAnim = NewObject<UPhysicalAnimationComponent>(GetOwner());
    PhysicalAnim->RegisterComponent();
    PhysicalAnim->SetSkeletalMeshComponent(MeshComponent);

    FPhysicalAnimationData Profile;
    Profile.bIsLocalSimulation = true;
    Profile.OrientationStrength = LegOrientationStrength;
    for (const FName& Bone : LegRootBones)
    {
        PhysicalAnim->ApplyPhysicalAnimationSettingsBelow(Bone, Profile);
    }

    LimbContacts.SetNum(static_cast<int32>(EACFFreeClimbLimb::Count));
    InitializeStates();
    TransitionTo(EACFFreeClimbState::Idle);
    
    if (UsesRemoteState())
    {
        const FACFFreeClimbRepState& JumpCommon = ReplicatedJumpState.CommonState;
        
        const bool bCurrentStateIsJump = ReplicatedClimbState.State == EACFFreeClimbState::Jump && 
            JumpCommon.State == EACFFreeClimbState::Jump && 
                JumpCommon.SessionId == ReplicatedClimbState.SessionId && 
                    JumpCommon.Sequence == ReplicatedClimbState.Sequence;
        
        if (bCurrentStateIsJump) { OnRep_JumpState(); }
        else if (ReplicatedClimbState.SessionId != 0) { OnRep_ClimbState(); }
                    
    }
    
    RefreshSurfaceTickPrerequisite();
}

void UACFFreeClimbComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearSurfaceTickPrerequisite();
    ACFFreeClimbAnimInstance = nullptr;
    
    if (MeshComponent)
    {
        MeshComponent->RemoveTickPrerequisiteComponent(this);
    }
    
    Super::EndPlay(EndPlayReason);
}

void UACFFreeClimbComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME_CONDITION(UACFFreeClimbComponent, ReplicatedClimbState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UACFFreeClimbComponent, ReplicatedJumpState, COND_SkipOwner);
}

void UACFFreeClimbComponent::InitializeStates()
{
    States.Add(EACFFreeClimbState::Idle, MakeUnique<FACFFreeClimbState_Idle>(*this));
    States.Add(EACFFreeClimbState::Entering, MakeUnique<FACFFreeClimbState_Entering>(*this));
    States.Add(EACFFreeClimbState::Climb, MakeUnique<FACFFreeClimbState_Climb>(*this));
    States.Add(EACFFreeClimbState::Jump, MakeUnique<FACFFreeClimbState_Jump>(*this));
    States.Add(EACFFreeClimbState::Cling, MakeUnique<FACFFreeClimbState_Cling>(*this));
    States.Add(EACFFreeClimbState::ClimbUp, MakeUnique<FACFFreeClimbState_ClimbUp>(*this));
    States.Add(EACFFreeClimbState::ClimbExit, MakeUnique<FACFFreeClimbState_ClimbExit>(*this));
}

void UACFFreeClimbComponent::TransitionTo(EACFFreeClimbState NextState, const FACFSurfaceHitResult* InTransitionHit)
{
    if (CurrentState && ClimbState == NextState)
    {
        return;
    }

    TUniquePtr<IACFFreeClimbState>* Next = States.Find(NextState);
    if (!Next || !Next->Get() || Next->Get()->GetId() != NextState)
    {
        ensureMsgf(false, TEXT("Invalid free climb state: %d"), static_cast<int32>(NextState));
        return;
    }
    
    const bool bHasPreviousState = CurrentState != nullptr;
    // Both flags must be read here: ClimbState is overwritten with NextState further down.
    const EACFFreeClimbState PreviousState = ClimbState;
    const bool bStartingClimbSession = bHasPreviousState && PreviousState == EACFFreeClimbState::Idle && NextState != EACFFreeClimbState::Idle;
    const bool bEndingClimbSession = bHasPreviousState && PreviousState != EACFFreeClimbState::Idle && NextState == EACFFreeClimbState::Idle;
    if (OwnerCharacter && bStartingClimbSession)
    {
        bPrevReplicateMovement = OwnerCharacter->IsReplicatingMovement();
    }
    
    bHasTransitionHit  = InTransitionHit != nullptr;
    if (InTransitionHit) TransitionHit = *InTransitionHit;
    
    TransitionTarget = NextState;
    
    if (CurrentState) CurrentState->OnExit(); 

    CurrentState = Next->Get();
    ClimbState = NextState;
    
    ApplyMovementReplicationPolicy(ClimbState);
    
    if (ShouldSendState())
    {
        LocalStateStartServerTimeSeconds = static_cast<float>(ACFFreeClimbNetwork::GetServerTimeSeconds(GetWorld()));
    }
    
    CurrentState->OnEnter();
    TransitionTarget = ClimbState;
    
    if (bHasPreviousState &&
        ShouldSendState() &&
        LocalSessionId != 0 &&
        NextState != EACFFreeClimbState::Jump)
    {
        SendRepState(true);
    }
    if (NextState == EACFFreeClimbState::Idle)
    {
        if (bHasPreviousState)
        {
            MovementComponent->SetMovementMode(bPendingExitToWalking ? MOVE_Walking : MOVE_Falling);
        }
        ResetClimbRuntime();
        bPendingExitToWalking = false;
    }

    // Notified last, so listeners observe a settled component. Clinging is nested inside the climb
    // session: it opens after the session started and closes before the session ends.
    const bool bWasClinging = bHasPreviousState && PreviousState == EACFFreeClimbState::Cling;
    const bool bIsClinging = ClimbState == EACFFreeClimbState::Cling;

    if (bStartingClimbSession)
    {
        OnClimbingChanged.Broadcast(true);
    }

    if (bWasClinging != bIsClinging)
    {
        OnClingingChanged.Broadcast(bIsClinging);
    }

    if (bEndingClimbSession)
    {
        OnClimbingChanged.Broadcast(false);
    }
}

bool UACFFreeClimbComponent::ConsumeTransitionHit(FACFSurfaceHitResult& OutHit)
{
    if (!bHasTransitionHit)
    {
        return false;
    }

    OutHit = TransitionHit;
    TransitionHit = FACFSurfaceHitResult{};
    bHasTransitionHit = false;
    return true;
}

void UACFFreeClimbComponent::ResetClimbRuntime()
{
    ClimbInput = FVector2D::ZeroVector;
    PendingClimbInput = FVector2D::ZeroVector;
    bJumpPressed = false;
    PendingDropVelocity = FVector::ZeroVector;
    LastDesiredClimbDirection = FVector::ZeroVector;
    CurrentSurface = FACFFreeClimbSurface();
    AnchorClimbable = nullptr;
    ClearSurfaceTickPrerequisite();
    AnchorTriangleIndex = INDEX_NONE;
    AnchorBarycentric = FVector::ZeroVector;
    AnchorSource = EACFSurfaceSource::Skeletal;
    AnchorPrimitive.Reset();
    AnchorLocalPos = FVector::ZeroVector;
    AnchorLocalNormal = FVector::ZeroVector;
    bAnchorValid = false;
    AnchorRotationOffset  = FQuat::Identity;
    bHasAnchorRotationOffset = false;
}

void UACFFreeClimbComponent::RefreshSurfaceTickPrerequisite()
{
    USkeletalMeshComponent* NewSurfaceMesh = nullptr;
    
    if (AnchorSource == EACFSurfaceSource::Skeletal && AnchorClimbable)
    {
        NewSurfaceMesh = AnchorClimbable->GetSurfaceMeshComponent();
    }
    if (NewSurfaceMesh == MeshComponent) { NewSurfaceMesh = nullptr; }
    if (SurfaceTickPrerequisiteMesh.Get() == NewSurfaceMesh) { return; }
    
    ClearSurfaceTickPrerequisite();
    
    if (NewSurfaceMesh)
    {
        AddTickPrerequisiteComponent(NewSurfaceMesh);
        SurfaceTickPrerequisiteMesh = NewSurfaceMesh;
    }
}

void UACFFreeClimbComponent::ClearSurfaceTickPrerequisite()
{
    if (USkeletalMeshComponent* PreviousSurfaceMesh = SurfaceTickPrerequisiteMesh.Get())
    {
        RemoveTickPrerequisiteComponent(PreviousSurfaceMesh);
    }
    
    SurfaceTickPrerequisiteMesh.Reset();
}

bool UACFFreeClimbComponent::IsNetworkedGame() const
{
    const UWorld* World = GetWorld();
    return World && World->GetNetMode() != NM_Standalone;
}

bool UACFFreeClimbComponent::UsesRemoteState() const
{
    return IsNetworkedGame() && OwnerCharacter && !OwnerCharacter->IsLocallyControlled();
}

bool UACFFreeClimbComponent::ShouldSendState() const
{
    return IsNetworkedGame() && OwnerCharacter && OwnerCharacter->IsLocallyControlled();
}

bool UACFFreeClimbComponent::ConsumeMovementSendTick(float DeltaTime)
{
    MovementStateSendAccumulator += DeltaTime;
    
    const float SendInterval = 1.0f / MovementStateSendRate;
    if (MovementStateSendAccumulator < SendInterval) return false;
    
    MovementStateSendAccumulator = FMath::Fmod(MovementStateSendAccumulator, SendInterval);
    
    return true;
}

float UACFFreeClimbComponent::GetRemoteInterpolationDelay() const
{
    return OwnerCharacter && OwnerCharacter->HasAuthority() ? 0.0f : RemoteInterpolationDelay;
}

void UACFFreeClimbComponent::BeginLocalSession()
{
    ++LocalSessionId;
    if (LocalSessionId == 0) { ++LocalSessionId; }
    LocalSequence = 0;
}

FACFFreeClimbRepState UACFFreeClimbComponent::BuildRepState()
{
    ++LocalSequence;
    if (LocalSequence == 0) { LocalSequence++; }
    
    FACFFreeClimbRepState State;
    
    State.SessionId = LocalSessionId;
    State.Sequence = LocalSequence;
    State.State = ClimbState;
    State.StateStartServerTimeSeconds = LocalStateStartServerTimeSeconds;
    
    if (bAnchorValid)
    {
        State.Source = AnchorSource;
        if (AnchorSource == EACFSurfaceSource::Skeletal && AnchorClimbable)
        {
            State.ClimbableActor = AnchorClimbable->GetOwner();
            State.AnchorTriangleIndex = AnchorTriangleIndex;
            State.AnchorBarycentric = AnchorBarycentric;
        }
        else
        {
            State.StaticPrimitive = AnchorPrimitive.Get();
            State.StaticLocalPosition = AnchorLocalPos;
            State.StaticLocalNormal = AnchorLocalNormal;
        }
    }
    
    CurrentState->FillRepState(State);
    
    return State;
}

void UACFFreeClimbComponent::SendRepState(bool bReliable)
{
    if (!OwnerCharacter || LocalSessionId == 0) { return; }
    if (bReliable) { MovementStateSendAccumulator = 0.0f; }
    
    FACFFreeClimbRepState State = BuildRepState();
    
    if (OwnerCharacter->HasAuthority())
    {
        if (AcceptServerState(State)) { ApplyServerState(State); }
        return;
    }
    
    if (bReliable) { ServerSendClimbStateReliable(State); }
    else { ServerSendClimbStateUnreliable(State); }
}

void UACFFreeClimbComponent::SendJumpRepState(FACFJumpRepState State)
{
    if (!OwnerCharacter || LocalSessionId == 0) return;
    if (OwnerCharacter->HasAuthority())
    {
        if (AcceptServerJumpState(State))
        {
            ApplyServerJumpState(State);
        }
        return;
    }
    
    ServerSendJumpStateReliable(MoveTemp(State));
}

void UACFFreeClimbComponent::ServerSendClimbStateReliable_Implementation(FACFFreeClimbRepState State)
{
    if (AcceptServerState(State)) { ApplyServerState(State); }
}

void UACFFreeClimbComponent::ServerSendClimbStateUnreliable_Implementation(FACFFreeClimbRepState State)
{
    if (AcceptServerState(State)) { ApplyServerState(State); }
}

void UACFFreeClimbComponent::ServerSendJumpStateReliable_Implementation(FACFJumpRepState State)
{
    if (AcceptServerJumpState(State)) { ApplyServerJumpState(State); }
}

bool UACFFreeClimbComponent::AcceptServerState(FACFFreeClimbRepState& State)
{
    if (!OwnerCharacter || !OwnerCharacter->HasAuthority() || !GetWorld()) return false;
    if (!IsValidClimbState(State.State)) return false;
    if (State.SessionId == 0 || State.Sequence == 0) return false;
    
    if (State.State != EACFFreeClimbState::Idle)
    {
        if (State.Source == EACFSurfaceSource::Skeletal)
        {
            const UACFFreeClimbableComponent* Climbable = FindReplicatedClimbable(State);
            if (State.AnchorTriangleIndex == INDEX_NONE || !Climbable || !ShouldAcceptClimbableActor(Climbable->GetOwner())) return false;
        }
        else
        {
            if (!AcceptsClimbablePrimitive(State.StaticPrimitive)) return false;
        }
    }
    
    if (State.ClimbRotation.ContainsNaN() || State.ClimbRotation.SizeSquared() <= KINDA_SMALL_NUMBER) { return false; }
    
    if (State.State == EACFFreeClimbState::Cling && !State.bHasAnchorRelativeLocation) return false;
        
    const bool bNewSession = State.SessionId != ReplicatedClimbState.SessionId;
    if (bNewSession)
    {
        if (ReplicatedClimbState.SessionId != 0 && !ACFFreeClimbNetwork::IsSequenceNewer(State.SessionId, ReplicatedClimbState.SessionId)) return false;
        if (State.State != EACFFreeClimbState::Entering) return false;
    }
    else if (!ACFFreeClimbNetwork::IsSequenceNewer(State.Sequence, ReplicatedClimbState.Sequence)) return false;
    
    const float ServerNow = static_cast<float>(ACFFreeClimbNetwork::GetServerTimeSeconds(GetWorld()));
    const bool bStateChanged = bNewSession || State.State != ReplicatedClimbState.State;
    
    State.ServerTimeSeconds = ServerNow;
    if (!bStateChanged)
    {
        State.StateStartServerTimeSeconds = ReplicatedClimbState.StateStartServerTimeSeconds;
    }
    else if (State.StateStartServerTimeSeconds <= 0.0f)
    {
        State.StateStartServerTimeSeconds = ServerNow;
    }
    State.MoveInput = State.MoveInput.GetClampedToMaxSize(1.0f);
    if (State.State != EACFFreeClimbState::Climb && State.State != EACFFreeClimbState::Cling)
    {
        State.MoveInput = FVector2D::ZeroVector;
    }
    if (State.State != EACFFreeClimbState::Cling)
    {
        State.bHasAnchorRelativeLocation = false;
        State.AnchorRelativeLocation = FVector::ZeroVector;
    }
    State.bExitToWalking = State.State == EACFFreeClimbState::Idle && State.bExitToWalking;

    // A drop only releases into the air, and only ever at the configured speed: a client cannot use
    // the release to launch itself.
    if (State.State != EACFFreeClimbState::Idle || State.bExitToWalking || State.DropVelocity.ContainsNaN())
    {
        State.DropVelocity = FVector::ZeroVector;
    }
    else
    {
        State.DropVelocity = State.DropVelocity.GetClampedToMaxSize(DropPushBackSpeed);
    }

    State.ClimbRotation.Normalize();
    
    return true;
}

void UACFFreeClimbComponent::ApplyServerState(const FACFFreeClimbRepState& State)
{
    ReplicatedClimbState = State;
    
    if (UsesRemoteState()) { ApplyRemoteState(State); }
    if (OwnerCharacter) { OwnerCharacter->ForceNetUpdate(); }
}

bool UACFFreeClimbComponent::AcceptServerJumpState(FACFJumpRepState& State)
{
    FACFFreeClimbRepState& Common = State.CommonState;
    
    if (Common.State != EACFFreeClimbState::Jump || State.Path.Num() < 2 || State.Path.Num() > JumpSegment + 1) return false;
    
    const FACFJumpPathPoint& StartPoint = State.Path[0];
    if (StartPoint.Source != Common.Source) return false;

    if (Common.Source == EACFSurfaceSource::Skeletal)
    {
        if (StartPoint.ClimbableActor != Common.ClimbableActor ||
            StartPoint.TriangleIndex != Common.AnchorTriangleIndex ||
            !StartPoint.Barycentric.Equals(Common.AnchorBarycentric, KINDA_SMALL_NUMBER))
        {
            return false;
        }
    }
    else
    {
        if (StartPoint.StaticPrimitive != Common.StaticPrimitive) return false;
    }

    for (const FACFJumpPathPoint& Point : State.Path)
    {
        if (!Point.IsValid()) return false;
    }

    if (!AcceptServerState(Common)) return false;
    
    State.Duration = JumpDuration;
    
    return true;
}

void UACFFreeClimbComponent::ApplyServerJumpState(const FACFJumpRepState& State)
{
    ReplicatedJumpState = State;
    ReplicatedClimbState = State.CommonState;
    
    if (UsesRemoteState()) { ApplyRemoteJumpState(State); }
    if (OwnerCharacter) { OwnerCharacter->ForceNetUpdate(); }
}

void UACFFreeClimbComponent::OnRep_ClimbState()
{
    if (!UsesRemoteState()) return;
    if (ReplicatedClimbState.State == EACFFreeClimbState::Jump) return;
    ApplyRemoteState(ReplicatedClimbState);
}

void UACFFreeClimbComponent::OnRep_JumpState()
{
    if (!UsesRemoteState()) return;
    
    const FACFFreeClimbRepState& Common = ReplicatedJumpState.CommonState;
    if (Common.State != EACFFreeClimbState::Jump || Common.SessionId == 0 || Common.Sequence == 0) return;
    
    if (ReplicatedClimbState.SessionId != 0)
    {
        if (ReplicatedClimbState.SessionId == Common.SessionId)
        {
            if (ACFFreeClimbNetwork::IsSequenceNewer(ReplicatedClimbState.Sequence, Common.Sequence)) return;
        }
        else if (!ACFFreeClimbNetwork::IsSequenceNewer(Common.SessionId, ReplicatedClimbState.SessionId)) return;
    }
    
    ApplyRemoteJumpState(ReplicatedJumpState);
}

void UACFFreeClimbComponent::ApplyRemoteState(const FACFFreeClimbRepState& State)
{
    // Active -> IDLE
    if (State.State == EACFFreeClimbState::Idle)
    {
        if (RemoteSessionId == 0 || RemoteSessionId != State.SessionId) return;

        const bool bFinishMontageLocally  = (ClimbState == EACFFreeClimbState::ClimbUp || ClimbState == EACFFreeClimbState::ClimbExit) && State.bExitToWalking;

        RemoteSessionId = 0;
        RemoteVisualMoveInput = FVector2D::ZeroVector;

        if (bFinishMontageLocally ) return;

        bPendingExitToWalking = State.bExitToWalking;
        if (ClimbState != EACFFreeClimbState::Idle) { TransitionTo(EACFFreeClimbState::Idle); }

        CurrentState->ApplyRemoteState(State);
        return;
    }

    const bool bNewSession = RemoteSessionId == 0 || RemoteSessionId != State.SessionId;
    if (bNewSession)
    {
        if (ClimbState != EACFFreeClimbState::Idle) { TransitionTo(EACFFreeClimbState::Idle); }
        RemoteSessionId = State.SessionId;
    }

    AnchorSource = State.Source;
    if (State.Source == EACFSurfaceSource::Skeletal)
    {
        UACFFreeClimbableComponent* Climbable = FindReplicatedClimbable(State);
        if (!Climbable || State.AnchorTriangleIndex == INDEX_NONE) return;

        AnchorClimbable = Climbable;
        AnchorTriangleIndex = State.AnchorTriangleIndex;
        AnchorBarycentric = State.AnchorBarycentric;
        AnchorPrimitive.Reset();
    }
    else
    {
        if (!State.StaticPrimitive) return;

        AnchorPrimitive = State.StaticPrimitive;
        AnchorLocalPos = State.StaticLocalPosition;
        AnchorLocalNormal = State.StaticLocalNormal;
        AnchorTriangleIndex = INDEX_NONE;
    }

    bAnchorValid = true;

    if (ClimbState != State.State) { TransitionTo(State.State); }
    CurrentState->ApplyRemoteState(State);
}

void UACFFreeClimbComponent::ApplyRemoteJumpState(const FACFJumpRepState& State)
{
    const FACFFreeClimbRepState& Common = State.CommonState;
    if (Common.State != EACFFreeClimbState::Jump || State.Path.Num() < 2) return;

    const bool bNewSession = RemoteSessionId == 0 || RemoteSessionId != Common.SessionId;
    if (bNewSession)
    {
        if (ClimbState != EACFFreeClimbState::Idle) { TransitionTo(EACFFreeClimbState::Idle); }
        RemoteSessionId = Common.SessionId;
    }

    AnchorSource = Common.Source;
    if (Common.Source == EACFSurfaceSource::Skeletal)
    {
        UACFFreeClimbableComponent* Climbable = FindReplicatedClimbable(Common);
        if (!Climbable || Common.AnchorTriangleIndex == INDEX_NONE) return;

        AnchorClimbable = Climbable;
        AnchorTriangleIndex = Common.AnchorTriangleIndex;
        AnchorBarycentric = Common.AnchorBarycentric;
        AnchorPrimitive.Reset();
    }
    else
    {
        if (!Common.StaticPrimitive) return;

        AnchorPrimitive = Common.StaticPrimitive;
        AnchorLocalPos = Common.StaticLocalPosition;
        AnchorLocalNormal = Common.StaticLocalNormal;
        AnchorTriangleIndex = INDEX_NONE;
        AnchorClimbable = nullptr;
    }

    bAnchorValid = true;
    
    if (ClimbState != EACFFreeClimbState::Jump)
    {
        TransitionTo(EACFFreeClimbState::Jump);
    }
    
    FACFFreeClimbState_Jump* JumpState = static_cast<FACFFreeClimbState_Jump*>(CurrentState);
    
    JumpState->ApplyRemoteJumpState(State);
}

UACFFreeClimbableComponent* UACFFreeClimbComponent::FindReplicatedClimbable(const FACFFreeClimbRepState& State) const
{
    return State.ClimbableActor ? State.ClimbableActor->FindComponentByClass<UACFFreeClimbableComponent>() : nullptr;
}

void UACFFreeClimbComponent::PreventCharacterMoveCombining() const
{
    if (!OwnerCharacter || !MovementComponent || OwnerCharacter->HasAuthority() || !OwnerCharacter->IsLocallyControlled())
    {
        return;
    }

    FNetworkPredictionData_Client_Character* ClientData = MovementComponent->GetPredictionData_Client_Character();

    if (ClientData && ClientData->PendingMove.IsValid())
    {
        ClientData->PendingMove->bForceNoCombine = true;
    }
}

void UACFFreeClimbComponent::ApplyMovementReplicationPolicy(EACFFreeClimbState State)
{
    if (!OwnerCharacter || !IsNetworkedGame()) return;
    
    if (OwnerCharacter->HasAuthority())
    {
        bool bShouldReplicateMovement = bPrevReplicateMovement;
        
        switch (State)
        {
        case EACFFreeClimbState::Entering:
        case EACFFreeClimbState::Climb:
        case EACFFreeClimbState::Jump:
        case EACFFreeClimbState::Cling:
        case EACFFreeClimbState::ClimbUp:
            bShouldReplicateMovement = false;
            break;
            
        case EACFFreeClimbState::ClimbExit:
            bShouldReplicateMovement = true;
            break;
            
        case EACFFreeClimbState::Idle:
        default:
            bShouldReplicateMovement = bPrevReplicateMovement;
            break;
        }
        
        if (OwnerCharacter->IsReplicatingMovement() != bShouldReplicateMovement)
        {
            OwnerCharacter->SetReplicateMovement(bShouldReplicateMovement);
            OwnerCharacter->ForceNetUpdate();
        }
    }
    
    EnsureAutonomousProxyMovementReplication();
}

// autonomous proxy replicated movement on
void UACFFreeClimbComponent::EnsureAutonomousProxyMovementReplication() const
{
    if (!OwnerCharacter ||
        !IsNetworkedGame() ||
        ClimbState == EACFFreeClimbState::Idle ||
        OwnerCharacter->GetLocalRole() != ROLE_AutonomousProxy ||
        !OwnerCharacter->IsLocallyControlled() ||
        OwnerCharacter->IsReplicatingMovement())
    {
        return;
    }
    
    OwnerCharacter->SetReplicateMovement(true);
}

void UACFFreeClimbComponent::EnterClimbState()
{
    if (bClimbModeActive) return;

    bPrevOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
    bPrevUseControllerRotationYaw = OwnerCharacter->bUseControllerRotationYaw;
    bPrevIgnoreClientMovementErrorChecksAndCorrection = MovementComponent->bIgnoreClientMovementErrorChecksAndCorrection;
    bPrevClientIgnoreMovementCorrections = MovementComponent->bClientIgnoreMovementCorrections;

    MovementComponent->StopMovementImmediately();
    MovementComponent->SetMovementMode(MOVE_None);
    MovementComponent->bOrientRotationToMovement = false;
    MovementComponent->bIgnoreClientMovementErrorChecksAndCorrection = true;
    MovementComponent->bClientIgnoreMovementCorrections = true;
    
    OwnerCharacter->bUseControllerRotationYaw = false;
    
    PreventCharacterMoveCombining();
    
    bClimbModeActive = true;
}

void UACFFreeClimbComponent::ExitClimbState()
{
    if (!bClimbModeActive) return;
    
    MovementComponent->bIgnoreClientMovementErrorChecksAndCorrection = bPrevIgnoreClientMovementErrorChecksAndCorrection;
    MovementComponent->bClientIgnoreMovementCorrections = bPrevClientIgnoreMovementCorrections;
    MovementComponent->bOrientRotationToMovement = bPrevOrientRotationToMovement;
    OwnerCharacter->bUseControllerRotationYaw = bPrevUseControllerRotationYaw;

    for (FACFFreeClimbContact& Contact : LimbContacts)
    {
        Contact.Reset();
    }

    bClimbModeActive = false;
}

bool UACFFreeClimbComponent::CacheOwnerComponents()
{
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        UE_LOG(LogACFFreeClimb, Warning, TEXT("ClimbingSystem disabled. Owner is not ACharacter."));
        return false;
    }

    CapsuleComponent  = OwnerCharacter->GetCapsuleComponent();
    MovementComponent = OwnerCharacter->GetCharacterMovement();
    MeshComponent     = OwnerCharacter->GetMesh();

    if (!CapsuleComponent || !MovementComponent || !MeshComponent)
    {
        UE_LOG(LogACFFreeClimb, Warning, TEXT("ClimbingSystem disabled. Required character components missing."));
        return false;
    }

    return true;
}

void UACFFreeClimbComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Latch the input Blueprint pushed since the last tick and clear it: Enhanced Input keeps
    // firing Triggered every frame while the axis is held, so an input that stops arriving means
    // the stick was released. Without this the last pushed value would stick and the character
    // would keep climbing on its own.
    ClimbInput = PendingClimbInput.GetClampedToMaxSize(1.0f);
    PendingClimbInput = FVector2D::ZeroVector;

    RefreshSurfaceTickPrerequisite();
    EnsureAutonomousProxyMovementReplication();

    if (!OwnerCharacter || !MovementComponent || !CurrentState) return;

    const bool bAuthorityClimbUp =
        OwnerCharacter->HasAuthority() &&
        ClimbState == EACFFreeClimbState::ClimbUp;

    if (UsesRemoteState() && !bAuthorityClimbUp)
    {
        CurrentState->TickRemote(DeltaTime);
        return;
    }

    CurrentState->Tick(DeltaTime);

    RefreshSurfaceTickPrerequisite();
}

bool UACFFreeClimbComponent::StartClimbing()
{
    if (UsesRemoteState()) return false;
    if (ClimbState != EACFFreeClimbState::Idle) return false;

    // A drop just released this surface: refuse to grab it again until the character has had time
    // to fall clear, otherwise the push back is undone by an immediate re-attach.
    if (GetWorld()->GetTimeSeconds() - LastDropTimeSeconds < DropRegrabDelay) return false;

    FACFSurfaceHitResult Hit;
    if (QuerySurface(Hit) && IsClimbable(Hit) && Hit.IsValidSurface())
    {
        const FVector Forward = OwnerCharacter->GetActorForwardVector();
        const float ProbeLength = CapsuleComponent->GetScaledCapsuleRadius() + GrabProbeDistance;

        const float EyeProbeLength = 2.0f * OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const FVector AnchorProbeOrigin = CapsuleComponent->GetComponentTransform().TransformPosition(AnchorProbeOffset);
        const FVector EyeStart = AnchorProbeOrigin + OwnerCharacter->GetActorUpVector() * EyeProbeLength;
        const FVector EyeEnd = EyeStart + Forward * ProbeLength * 2.0f;

        FACFSurfaceHitResult EyeHit;
        if (QuerySurfaceSegment(EyeStart, EyeEnd, EyeHit))
        {
            TransitionTo(EACFFreeClimbState::Entering, &Hit);
            return true;
        }
    }
    return false;
}

void UACFFreeClimbComponent::StopClimbing(EACFFreeClimbStopReason Reason)
{
    static_cast<void>(Reason);
    if (UsesRemoteState()) return;
    if (ClimbState == EACFFreeClimbState::Idle) return;

    bPendingExitToWalking = false;
    TransitionTo(EACFFreeClimbState::Idle);
}

void UACFFreeClimbComponent::DropFromSurface()
{
    if (UsesRemoteState()) return;
    if (ClimbState == EACFFreeClimbState::Idle) return;

    // Read before the transition: going Idle resets the surface runtime.
    const FVector PushDirection = CurrentSurface.bValid
        ? CurrentSurface.SmoothedNormal.GetSafeNormal()
        : -OwnerCharacter->GetActorForwardVector();

    // The climb session leaves the capsule stationary, so the fall would start glued to the wall.
    const FVector DropVelocity = PushDirection * DropPushBackSpeed;

    // Staged before the transition: TransitionTo builds and sends the Idle rep state, which carries
    // this velocity so the authority starts the identical fall instead of correcting it away.
    PendingDropVelocity = DropVelocity;

    // Falling, not walking: the drop is a release into the air even when a floor is close.
    bPendingExitToWalking = false;
    TransitionTo(EACFFreeClimbState::Idle);

    LastDropTimeSeconds = GetWorld()->GetTimeSeconds();

    MovementComponent->Velocity = DropVelocity;
}

void UACFFreeClimbComponent::StartJumpMovement()
{
    if (CurrentState) CurrentState->StartJumpMovement();
}

void UACFFreeClimbComponent::StartCling()
{
    if (UsesRemoteState()) return;
    if (ClimbState != EACFFreeClimbState::Climb) return;

    TransitionTo(EACFFreeClimbState::Cling);
}

void UACFFreeClimbComponent::StopCling()
{
    if (UsesRemoteState()) return;
    if (ClimbState != EACFFreeClimbState::Cling) return;

    TransitionTo(EACFFreeClimbState::Entering);
}

void UACFFreeClimbComponent::OnClimbUpMontageNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
    if (ClimbState != EACFFreeClimbState::ClimbUp || !CurrentState) return;
    static_cast<FACFFreeClimbState_ClimbUp*>(CurrentState)->HandleMontageNotify(NotifyName);
}

void UACFFreeClimbComponent::OnClimbUpMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (ClimbState != EACFFreeClimbState::ClimbUp) return;
    bPendingExitToWalking = !bInterrupted;
    TransitionTo(EACFFreeClimbState::Idle);
}

void UACFFreeClimbComponent::OnClimbExitMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (ClimbState == EACFFreeClimbState::ClimbExit)
    {
        static_cast<FACFFreeClimbState_ClimbExit*>(CurrentState)->HandleMontageEnded(Montage, bInterrupted);
    }
}

bool UACFFreeClimbComponent::QuerySurface(FACFSurfaceHitResult& OutHit) const
{
    const FVector Forward = OwnerCharacter->GetActorForwardVector();
    const FVector Start   = CapsuleComponent->GetComponentTransform().TransformPosition(AnchorProbeOffset);
    const FVector End     = Start + Forward * (CapsuleComponent->GetScaledCapsuleRadius() + GrabProbeDistance);
    return QuerySurfaceSegment(Start, End, OutHit);
}

bool UACFFreeClimbComponent::QuerySurfaceAt(const FVector& Point, const FVector& Normal, FACFSurfaceHitResult& OutHit) const
{
    const FVector N     = Normal.GetSafeNormal();
    const FVector Start = Point + N * SurfaceProbeStartOffset;
    const FVector End   = Point - N * SurfaceProbeDistance;

    return QuerySurfaceSegment(Start, End, OutHit);
}

bool UACFFreeClimbComponent::QuerySurfaceSegment(const FVector& Start, const FVector& End, FACFSurfaceHitResult& OutHit) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(ACFFreeClimb_QuerySurface);
    DrawRaycastQuery(Start, End);

    FACFSurfaceHitResult SkeletalHit;
    FACFSurfaceHitResult StaticHit;
    const bool bSkeletal = QuerySkeletalSegment(Start, End, SkeletalHit);
    const bool bStatic = QueryStaticSegment(Start, End, StaticHit);

    if (bSkeletal && bStatic)
    {
        OutHit = (StaticHit.Distance < SkeletalHit.Distance) ? StaticHit : SkeletalHit;
        return true;
    }
    if (bSkeletal) { OutHit = SkeletalHit; return true; }
    if (bStatic) { OutHit = StaticHit;   return true; }
    return false;
}

bool UACFFreeClimbComponent::QuerySkeletalSegment(const FVector& Start, const FVector& End, FACFSurfaceHitResult& OutHit) const
{
    UWorld* World = GetWorld();
    if (!World) return false;

    UACFFreeClimbableWorldSubsystem* Subsystem = World->GetSubsystem<UACFFreeClimbableWorldSubsystem>();
    if (!Subsystem) return false;

    FACFSurfaceQuery Query;
    Query.Start = Start;
    Query.End = End;
    Query.bCullBackfaces = true;

    return Subsystem->QuerySurface(Query, GetOwner(), OutHit, [this](const AActor* Actor)
    {
        return ShouldAcceptClimbableActor(Actor);
    });
}

bool UACFFreeClimbComponent::QueryStaticSegment(const FVector& Start, const FVector& End, FACFSurfaceHitResult& OutHit) const
{
    UWorld* World = GetWorld();
    if (!World) return false;

    const FCollisionObjectQueryParams ObjectParams = MakeClimbObjectQuery(CollisionChannels);
    if (!ObjectParams.IsValid())
    {
        return false;
    }

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());

    TArray<FHitResult> Hits;
    if (!World->LineTraceMultiByObjectType(Hits, Start, End, ObjectParams, QueryParams))
    {
        return false;
    }

    for (const FHitResult& Hit : Hits)
    {
        if (!AcceptsClimbablePrimitive(Hit.GetComponent()))
        {
            continue;
        }

        FillStaticHitFromTrace(Hit, OutHit);
        return true;
    }

    return false;
}

bool UACFFreeClimbComponent::QueryCornerCandidates(const FVector& Start, const FVector& End, TArray<FACFSurfaceHitResult>& OutHits) const
{
    UWorld* World = GetWorld();
    if (!World)
        return false;

    UACFFreeClimbableWorldSubsystem* Subsystem = World->GetSubsystem<UACFFreeClimbableWorldSubsystem>();
    if (!Subsystem)
        return false;

    FACFSurfaceSphereSweepQuery Query;
    Query.Start = Start;
    Query.End = End;
    Query.Radius = CornerSearchRadius;

    return Subsystem->SweepSphereSurfaceMulti(Query, GetOwner(), OutHits, [this](const AActor* Actor)
    {
        return ShouldAcceptClimbableActor(Actor);
    });
}

bool UACFFreeClimbComponent::PassesActorTagFilter(const AActor* Actor) const
{
    for (const FName& Tag : RequiredActorTags)
    {
        if (Tag.IsNone())
        {
            continue;
        }
        if (!Actor || !Actor->ActorHasTag(Tag))
        {
            return false;
        }
    }

    if (!Actor)
    {
        return true;
    }

    for (const FName& Tag : BlockingActorTags)
    {
        if (!Tag.IsNone() && Actor->ActorHasTag(Tag))
        {
            return false;
        }
    }

    return true;
}

bool UACFFreeClimbComponent::HasClimbableCollisionChannel(ECollisionChannel Channel) const
{
    for (const TEnumAsByte<ECollisionChannel>& Entry : CollisionChannels)
    {
        if (Entry.GetValue() == Channel)
        {
            return true;
        }
    }
    return false;
}

bool UACFFreeClimbComponent::ShouldAcceptClimbableActor(const AActor* Actor) const
{
    return PassesActorTagFilter(Actor);
}

bool UACFFreeClimbComponent::AcceptsClimbablePrimitive(const UPrimitiveComponent* Primitive) const
{
    if (!IsValid(Primitive))
    {
        return false;
    }

    if (!HasClimbableCollisionChannel(Primitive->GetCollisionObjectType()))
    {
        return false;
    }

    return PassesActorTagFilter(Primitive->GetOwner());
}

bool UACFFreeClimbComponent::CanClimbActor(AActor* Actor) const
{
    return ShouldAcceptClimbableActor(Actor);
}

void UACFFreeClimbComponent::AddCollisionChannel(TEnumAsByte<ECollisionChannel> Channel)
{
    CollisionChannels.AddUnique(Channel);
}

void UACFFreeClimbComponent::RemoveCollisionChannel(TEnumAsByte<ECollisionChannel> Channel)
{
    CollisionChannels.Remove(Channel);
}

bool UACFFreeClimbComponent::IsClimbable(const FACFSurfaceHitResult& Hit) const
{
    if (Hit.Source == EACFSurfaceSource::Static)
    {
        if (!AcceptsClimbablePrimitive(Hit.HitComponent))
        {
            return false;
        }
    }
    else
    {
        const AActor* Actor = Hit.Climbable ? Hit.Climbable->GetOwner() : nullptr;
        if (!Actor && Hit.HitComponent)
        {
            Actor = Hit.HitComponent->GetOwner();
        }
        if (!ShouldAcceptClimbableActor(Actor))
        {
            return false;
        }
    }

    const FVector Normal = Hit.Normal.GetSafeNormal();

    const float SurfaceUpDot = FVector::DotProduct(Normal, FVector::UpVector);
    const float MaxUpDot     = FMath::Sin(FMath::DegreesToRadians(MaxClimbAngle));
    if (FMath::Abs(SurfaceUpDot) > MaxUpDot) return false;

    const FVector Forward = OwnerCharacter->GetActorForwardVector();
    if (FVector::DotProduct(Forward, -Normal) < MinFacingDot) return false;

    return true;
}

void UACFFreeClimbComponent::UpdateSurfaceFromHit(const FACFSurfaceHitResult& Hit)
{
    CurrentSurface.Point = Hit.Position;
    CurrentSurface.RawNormal = Hit.Normal.GetSafeNormal();
    CurrentSurface.PoseTargetNormal = CurrentSurface.RawNormal;
    CurrentSurface.SmoothedNormal = CurrentSurface.RawNormal;
}

void UACFFreeClimbComponent::SetAnchorFromHit(const FACFSurfaceHitResult& Hit)
{
    AnchorSource = Hit.Source;

    if (Hit.Source == EACFSurfaceSource::Static)
    {
        AnchorPrimitive = Hit.HitComponent;

        const FTransform ComponentWorldTransform = Hit.HitComponent->GetComponentTransform();
        AnchorLocalPos = ComponentWorldTransform.InverseTransformPosition(Hit.Position);
        AnchorLocalNormal = ComponentWorldTransform.InverseTransformVectorNoScale(Hit.Normal.GetSafeNormal());

        AnchorClimbable = nullptr;
        AnchorTriangleIndex = INDEX_NONE;
        AnchorBarycentric = FVector::ZeroVector;

        bAnchorValid = (Hit.HitComponent != nullptr);
    }
    else
    {
        AnchorClimbable = Hit.Climbable;
        AnchorTriangleIndex = Hit.TriangleIndex;
        AnchorBarycentric = Hit.Barycentric;

        AnchorPrimitive = nullptr;

        bAnchorValid = (AnchorClimbable != nullptr && AnchorTriangleIndex != INDEX_NONE);
    }
    
    RefreshSurfaceTickPrerequisite();
}

bool UACFFreeClimbComponent::ResolveAnchorWorld(FVector& OutPoint, FVector& OutNormal) const
{
    if (!bAnchorValid) return false;

    if (AnchorSource == EACFSurfaceSource::Skeletal)
    {
        if (!AnchorClimbable) return false;

        FVector P, N;
        if (!AnchorClimbable->ResolveAnchor(AnchorTriangleIndex, AnchorBarycentric, P, N)) return false;

        OutPoint = P;
        OutNormal = N;
        return true;
    }
    else
    {
        UPrimitiveComponent* Prim = AnchorPrimitive.Get();
        if (!Prim) return false;

        const FTransform ComponentWorldTransform = Prim->GetComponentTransform();
        OutPoint = ComponentWorldTransform.TransformPosition(AnchorLocalPos);
        OutNormal = ComponentWorldTransform.TransformVectorNoScale(AnchorLocalNormal).GetSafeNormal();
        return true;
    }
}

void UACFFreeClimbComponent::UpdateProbeContacts()
{
    if (!OwnerCharacter || !MeshComponent)
    {
        return;
    }

    if (!bRigProfileValid || !RigProfile)
    {
        for (FACFFreeClimbContact& Contact : LimbContacts)
        {
            Contact.Reset();
        }
        return;
    }

    for (uint8 LimbValue = 0; LimbValue < static_cast<uint8>(EACFFreeClimbLimb::Count); ++LimbValue)
    {
        const EACFFreeClimbLimb Limb = static_cast<EACFFreeClimbLimb>(LimbValue);
        const FACFFreeClimbLimbProfile& LimbProfile = RigProfile->GetLimbProfile(Limb);
        const int32 LimbIndex = GetLimbIndex(Limb);

        if (!LimbProfile.bEnabled)
        {
            if (LimbContacts.IsValidIndex(LimbIndex)) LimbContacts[LimbIndex].Reset();
            continue;
        }

        FACFFreeClimbProbe Probe;
        if (!MakeProfileProbe(Limb, Probe))
        {
            for (FACFFreeClimbContact& Contact : LimbContacts)
            {
                Contact.Reset();
            }
            return;
        }
        FACFFreeClimbProbeResult Result;
        Result.ProbeName = Probe.ProbeName;
        CollectProbeHits(Probe, Result);
        ApplyLimbProbe(Probe, Result);
    }
}

bool UACFFreeClimbComponent::MakeProfileProbe(EACFFreeClimbLimb Limb, FACFFreeClimbProbe& OutProbe) const
{
    OutProbe = FACFFreeClimbProbe{};
    if (!RigProfile)
    {
        UE_LOG(LogACFFreeClimb, Warning,
            TEXT("Limb contact probing disabled for '%s'. ACFFreeClimbRigProfile is unavailable."),
            *GetNameSafe(GetOwner()));
        return false;
    }

    const FACFFreeClimbLimbProfile& LimbProfile = RigProfile->GetLimbProfile(Limb);
    OutProbe.ProbeName = *UEnum::GetValueAsString(Limb);
    OutProbe.Limb = Limb;
    OutProbe.LocalOffset = LimbProfile.ProbeLocalOffset;
    OutProbe.LocalDirection = LimbProfile.ProbeLocalDirection;
    OutProbe.ReachReferenceBone = LimbProfile.ProbeOriginBone;
    OutProbe.Radius = LimbProfile.ProbeRadius;
    OutProbe.TraceDistance = LimbProfile.TraceDistance;
    OutProbe.bUseFallbackFanSweep = LimbProfile.bUseFallbackFanSweep;
    OutProbe.FallbackFanPitchDegrees = LimbProfile.FallbackFanPitchDegrees;
    OutProbe.MinReach = LimbProfile.MinReach;
    OutProbe.MaxReach = LimbProfile.MaxReach;
    OutProbe.bEnabled = LimbProfile.bEnabled;
    return true;
}

void UACFFreeClimbComponent::CollectProbeHits(
    const FACFFreeClimbProbe& Probe,
    FACFFreeClimbProbeResult& OutResult) const
{
    if (!OwnerCharacter)
        return;
    const FTransform ActorTransform = OwnerCharacter->GetActorTransform();

    const FVector BaseStart = ActorTransform.TransformPosition(Probe.LocalOffset);

    const FVector2D Input = ClimbInput.GetClampedToMaxSize(1.0f);
    const FVector InputDirection = CurrentSurface.SurfaceRight * Input.X + CurrentSurface.SurfaceUp * Input.Y;

    const FVector Start = BaseStart + InputDirection * 20.0f;
    const FVector Dir = ActorTransform.TransformVectorNoScale(Probe.LocalDirection);
    const FVector End = Start + Dir * Probe.TraceDistance;

    TArray<FACFSurfaceHitResult> SweepHits;
    const auto CollectSweep = [this, &Probe, &SweepHits](const FVector& SweepStart, const FVector& SweepEnd)
    {
        FACFSurfaceSphereSweepQuery Query;
        Query.Start = SweepStart;
        Query.End = SweepEnd;
        Query.Radius = Probe.Radius;
        Query.bCullBackfaces = true;
        Query.RequiredQueryFlags = static_cast<uint8>(EACFSurfaceQueryFlags::IKProbe);

        if (UWorld* World = GetWorld())
        {
            if (UACFFreeClimbableWorldSubsystem* Subsystem =
                World->GetSubsystem<UACFFreeClimbableWorldSubsystem>())
            {
                TArray<FACFSurfaceHitResult> ClimbableHits;
                Subsystem->SweepSphereSurfaceMulti(Query, GetOwner(), ClimbableHits, [this](const AActor* Actor)
                {
                    return ShouldAcceptClimbableActor(Actor);
                });
                SweepHits.Append(ClimbableHits);
            }
        }

        SweepSphereStaticMulti(Query, SweepHits);
    };

    if (bDrawDebug)
    {
        DrawDebugLine(GetWorld(), Start, End, FColor::Red);
    }

    CollectSweep(Start, End);

    const auto HasReachableHit = [this, &Probe](const TArray<FACFSurfaceHitResult>& Hits)
    {
        if (!MeshComponent)
            return false;

        const FVector ReachOrigin = MeshComponent->GetSocketLocation(Probe.ReachReferenceBone);

        return Hits.ContainsByPredicate([&](const FACFSurfaceHitResult& Hit)
        {
            const float ReachDistance = FVector::Distance(Hit.Position, ReachOrigin);
            return ReachDistance >= Probe.MinReach && ReachDistance <= Probe.MaxReach;
        });
    };

    if (!HasReachableHit(SweepHits) && Probe.bUseFallbackFanSweep)
    {
        const FVector LocalRight = ActorTransform.TransformVectorNoScale(FVector::RightVector).GetSafeNormal();
        for (const float PitchDegrees : Probe.FallbackFanPitchDegrees)
        {
            const FVector FanDirection = Dir.RotateAngleAxis(PitchDegrees, LocalRight).GetSafeNormal();
            if (FanDirection.IsNearlyZero())
                continue;

            const FVector FanEnd = Start + FanDirection * Probe.TraceDistance * 1.5;
            if (bDrawDebug)
            {
                DrawDebugLine(GetWorld(), Start, FanEnd, FColor::Orange);
            }

            CollectSweep(Start, FanEnd);
        }
    }

    OutResult.Hits.Append(SweepHits);

    if (bDrawDebug)
    {
        for (const auto& Hit : SweepHits)
        {
            DrawDebugPoint(GetWorld(), Hit.Position, 8.0f, FColor::Red);
        }
    }
}
bool UACFFreeClimbComponent::SweepSphereStaticMulti(const FACFSurfaceSphereSweepQuery& Query, TArray<FACFSurfaceHitResult>& OutHits) const
{
    UWorld* World = GetWorld();
    if (!World)
        return false;

    const FCollisionObjectQueryParams ObjectParams = MakeClimbObjectQuery(CollisionChannels);
    if (!ObjectParams.IsValid())
    {
        return false;
    }

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());

    TArray<FHitResult> Hits;
    const bool bHit = World->SweepMultiByObjectType(
        Hits, Query.Start, Query.End, FQuat::Identity, ObjectParams,
        FCollisionShape::MakeSphere(Query.Radius), QueryParams);

    if (!bHit)
        return false;

    bool bAny = false;
    for (const FHitResult& H : Hits)
    {
        if (!AcceptsClimbablePrimitive(H.GetComponent()))
        {
            continue;
        }

        FACFSurfaceHitResult& S = OutHits.AddDefaulted_GetRef();
        S.Source = EACFSurfaceSource::Static;
        S.HitComponent = H.GetComponent();
        S.Climbable = nullptr;

        if (H.bStartPenetrating)
        {
            const FVector OutNormal = H.Normal.GetSafeNormal();
            if (OutNormal.IsNearlyZero())
            {
                OutHits.Pop();
                continue;
            }
            const float CenterToSurface = FMath::Max(Query.Radius - H.PenetrationDepth, 0.0f);
            S.Position = Query.Start - OutNormal * CenterToSurface;
            S.Normal = OutNormal;
            S.Distance = CenterToSurface;
        }
        else
        {
            S.Position = H.ImpactPoint;
            S.Normal = H.ImpactNormal;
            S.Distance = H.Distance;
        }
        bAny = true;
    }

    return bAny;
}

void UACFFreeClimbComponent::ApplyLimbProbe(const FACFFreeClimbProbe& Probe, const FACFFreeClimbProbeResult& Result)
{
    TArray<FACFFreeClimbContactCandidate> Candidates;
    Candidates.Reserve(Result.Hits.Num());

    for (const FACFSurfaceHitResult& Hit : Result.Hits)
    {
        FACFFreeClimbContactCandidate& Candidate = Candidates.AddDefaulted_GetRef();
        Candidate.SurfaceHit = Hit;
    }

    int32 LimbIndex = static_cast<int32>(Probe.Limb);

    FACFFreeClimbContact NewContact;
    if (!FindBestContact(Probe, Candidates, NewContact))
    {
        LimbContacts[LimbIndex].Reset();
        return;
    }

    LimbContacts[LimbIndex] = NewContact;

    if (bDrawDebug)
    {
        DrawDebugSphere(GetWorld(), NewContact.SurfaceHit.Position, 8.0f, 12, FColor::Yellow);
    }
}

bool UACFFreeClimbComponent::FindBestContact(
    const FACFFreeClimbProbe& Probe,
    const TArray<FACFFreeClimbContactCandidate>& Candidates,
    FACFFreeClimbContact& OutContact) const
{
    if (!MeshComponent) return false;

    const FVector ReachOrigin = MeshComponent->GetSocketLocation(Probe.ReachReferenceBone);

    float BestScore = -BIG_NUMBER;
    const FACFFreeClimbContactCandidate* BestCandidate = nullptr;

    for (const FACFFreeClimbContactCandidate& Candidate : Candidates)
    {
        const FVector ToCandidate = Candidate.SurfaceHit.Position - ReachOrigin;

        const float ReachDistance = ToCandidate.Size();
        if (ReachDistance < Probe.MinReach || ReachDistance > Probe.MaxReach)
            continue;

        float InputScore = 0.5f;
        if (!LastDesiredClimbDirection.IsNearlyZero())
        {
            const float Alignment = FVector::DotProduct(
                LastDesiredClimbDirection,
                ToCandidate.GetSafeNormal());

            InputScore = (Alignment + 1.0f) * 0.5f;
        }

        float DistanceScore = Candidate.SurfaceHit.Distance;

        const float Score = -DistanceScore;

        if (Score > BestScore)
        {
            BestScore = Score;
            BestCandidate = &Candidate;
        }
    }

    if (!BestCandidate)
        return false;

    OutContact.bValid = true;
    OutContact.SurfaceHit = BestCandidate->SurfaceHit;
    OutContact.Score = BestScore;

    return true;
}

bool UACFFreeClimbComponent::IsSurfacePersistentForPose(
    const FVector& SurfacePoint,
    const FVector& SurfaceNormal,
    const FVector& SurfaceMoveDir,
    const UACFFreeClimbableComponent* ExpectedClimbable) const
{
    if (MinPoseSurfaceTravelDistance <= KINDA_SMALL_NUMBER)
        return true;

    const FVector Normal = SurfaceNormal.GetSafeNormal();
    const FVector MoveDir = FVector::VectorPlaneProject(SurfaceMoveDir, Normal).GetSafeNormal();

    if (Normal.IsNearlyZero() || MoveDir.IsNearlyZero() || !ExpectedClimbable)
        return false;

    const FVector ProbePoint = SurfacePoint + MoveDir * MinPoseSurfaceTravelDistance;

    FACFSurfaceHitResult ProbeHit;
    if (!QuerySurfaceAt(ProbePoint, Normal, ProbeHit))
        return false;

    if (ProbeHit.Climbable != ExpectedClimbable || ProbeHit.TriangleIndex == INDEX_NONE)
        return false;

    const FVector ProbeNormal = ProbeHit.Normal.GetSafeNormal();
    const float MinNormalDot = FMath::Cos(FMath::DegreesToRadians(PoseSurfaceNormalToleranceDegrees));

    if (FVector::DotProduct(Normal, ProbeNormal) < MinNormalDot)
        return false;

    const float ActualProgress = FVector::DotProduct(ProbeHit.Position - SurfacePoint, MoveDir);
    const float RequiredProgress = MinPoseSurfaceTravelDistance * MinPoseSurfaceProgressRatio;
    return ActualProgress >= RequiredProgress;
}

void UACFFreeClimbComponent::UpdatePoseTargetNormal(
    const FVector& NewPoint,
    const FVector& NewNormal,
    const FVector& NewMoveDir,
    const UACFFreeClimbableComponent* NewClimbable)
{
    const FVector RawNormal = NewNormal.GetSafeNormal();
    if (RawNormal.IsNearlyZero())
        return;

    FVector& PoseTargetNormal = CurrentSurface.PoseTargetNormal;
    if (PoseTargetNormal.IsNearlyZero())
    {
        PoseTargetNormal = RawNormal;
        return;
    }

    // Static mesh transforms are authoritative for the local anchor normal.
    // Accept sharp normal changes directly.
    if (AnchorSource == EACFSurfaceSource::Static)
    {
        PoseTargetNormal = RawNormal;
        return;
    }

    const float NormalDot = FVector::DotProduct(PoseTargetNormal.GetSafeNormal(), RawNormal);
    const float NormalAngleDegrees = FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(NormalDot, -1.0f, 1.0f)));

    if (NormalAngleDegrees < SharpPoseNormalAngleDegrees ||
        IsSurfacePersistentForPose(NewPoint, RawNormal, NewMoveDir, NewClimbable))
    {
        PoseTargetNormal = RawNormal;
    }
}

FVector UACFFreeClimbComponent::ProjectSurfaceUp(const FVector& Normal) const
{
    FVector Up = FVector::VectorPlaneProject(FVector::UpVector, Normal);
    if (Up.Size() < MinMovementUpProjectionLength)
    {
        Up = FVector::VectorPlaneProject(OwnerCharacter->GetActorForwardVector(), Normal);
    }
    return Up.GetSafeNormal();
}

void UACFFreeClimbComponent::BuildSurfaceBasis(FACFFreeClimbSurface& Surface) const
{
    const FVector Normal = Surface.RawNormal.GetSafeNormal();

    Surface.SurfaceUp    = ProjectSurfaceUp(Normal);
    Surface.SurfaceRight = FVector::CrossProduct(Normal, Surface.SurfaceUp).GetSafeNormal();
}

FVector UACFFreeClimbComponent::GetAnchorAdjustedActorLocation(
    const FVector& AnchorPoint,
    const FVector& AnchorNormal,
    const FQuat& ActorRotation) const
{
    const FVector Normal = AnchorNormal.GetSafeNormal();
    const float Radius = CapsuleComponent->GetScaledCapsuleRadius();
    return AnchorPoint + Normal * Radius - ActorRotation.RotateVector(AnchorProbeOffset);
}

FTransform UACFFreeClimbComponent::ComputeClimbTransform() const
{
    const FVector Normal = CurrentSurface.SmoothedNormal;
    const FVector Up = ProjectSurfaceUp(Normal);
    const FQuat Rotation = FRotationMatrix::MakeFromXZ(-Normal, Up).ToQuat();
    const FVector Location = GetAnchorAdjustedActorLocation(CurrentSurface.Point, Normal, Rotation);

    return FTransform(Rotation, Location);
}

void UACFFreeClimbComponent::ApplyClimbTransform()
{
    const FTransform Target = ComputeClimbTransform();
    OwnerCharacter->SetActorLocationAndRotation(Target.GetLocation(), Target.GetRotation());
}

void UACFFreeClimbComponent::ApplyClimbTransform(const FQuat& Rotation)
{
    const FVector Normal = CurrentSurface.SmoothedNormal;
    const FVector Location = GetAnchorAdjustedActorLocation(CurrentSurface.Point, Normal, Rotation);
    
    OwnerCharacter->SetActorLocationAndRotation(Location, Rotation);
}

bool UACFFreeClimbComponent::ResolveAnchorRotation(FQuat& OutRotation) const
{
    if (!bAnchorValid) return false;
    
    if (AnchorSource == EACFSurfaceSource::Skeletal)
    {
        return AnchorClimbable && AnchorClimbable->ResolveTriangleFrameRotation(AnchorTriangleIndex, OutRotation);
    }
    
    UPrimitiveComponent* Primitive = AnchorPrimitive.Get();
    if (!Primitive) return false;
    
    OutRotation = Primitive->GetComponentQuat();
    
    return true;
}

bool UACFFreeClimbComponent::CaptureAnchorRotationOffset()
{
    FQuat AnchorRotation;
    if (!ResolveAnchorRotation(AnchorRotation))
    {
        bHasAnchorRotationOffset = false;
        return false;
    }
    
    const FTransform CharacterTransform(OwnerCharacter->GetActorQuat(), FVector::ZeroVector, FVector::OneVector);
    const FTransform AnchorTransform(AnchorRotation, FVector::ZeroVector, FVector::OneVector);
    AnchorRotationOffset = CharacterTransform.GetRelativeTransform(AnchorTransform).GetRotation().GetNormalized();
    
    bHasAnchorRotationOffset = true;
    return true;
}

bool UACFFreeClimbComponent::CalculateClimbRotation(float DeltaTime, const FVector2D& MoveInput, FQuat& OutRotation) const
{
    FQuat AnchorRotation;
    
    if (!bHasAnchorRotationOffset || !ResolveAnchorRotation(AnchorRotation)) return false;
    
    const FTransform RelativeTransform(AnchorRotationOffset, FVector::ZeroVector, FVector::OneVector);
    const FTransform AnchorTransform(AnchorRotation, FVector::ZeroVector, FVector::OneVector);
    
    FQuat TrackingRotation = (RelativeTransform * AnchorTransform).GetRotation().GetNormalized();
    
    if (!MoveInput.IsNearlyZero())
    {
        const FVector Normal = CurrentSurface.RawNormal.GetSafeNormal();
        const FVector WorldSurfaceUp = FVector::VectorPlaneProject(FVector::UpVector, Normal).GetSafeNormal();
        const FVector CharacterSurfaceUp = FVector::VectorPlaneProject(TrackingRotation.GetUpVector(), Normal).GetSafeNormal();
        
        if (!WorldSurfaceUp.IsNearlyZero() && !CharacterSurfaceUp.IsNearlyZero())
        {
            const float SignedAngle = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(CharacterSurfaceUp, WorldSurfaceUp), Normal),
                    FVector::DotProduct(CharacterSurfaceUp, WorldSurfaceUp));
            const float MaxAngleStep = FMath::DegreesToRadians(ClimbRotationSpeed) * DeltaTime;
            const float AngleStep = FMath::Clamp(SignedAngle, -MaxAngleStep, MaxAngleStep);

            TrackingRotation = (FQuat(Normal, AngleStep) * TrackingRotation).GetNormalized();
        }
    }
    
    OutRotation = TrackingRotation;
    return true;
}

FQuat UACFFreeClimbComponent::CalculateMovementSurfaceRotation(const FVector& ReferenceUp) const
{
    const FVector Normal = CurrentSurface.SmoothedNormal.GetSafeNormal();
    
    FVector SurfaceUp = FVector::VectorPlaneProject(ReferenceUp, Normal).GetSafeNormal();
    if (SurfaceUp.IsNearlyZero())
    {
        SurfaceUp = ProjectSurfaceUp(Normal);
    }
    
    return FRotationMatrix::MakeFromXZ(-Normal, SurfaceUp).ToQuat().GetNormalized();
}

void UACFFreeClimbComponent::DrawRaycastQuery(const FVector& Start, const FVector& End, const FColor Color) const
{
    if (!bDrawDebug) return;
    DrawDebugDirectionalArrow(GetWorld(), Start, End, 10.0f, Color, false, -1.0f, ESceneDepthPriorityGroup::SDPG_Foreground);
}
