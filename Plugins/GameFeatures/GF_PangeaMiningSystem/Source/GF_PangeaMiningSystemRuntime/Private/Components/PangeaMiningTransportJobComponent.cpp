#include "Components/PangeaMiningTransportJobComponent.h"

#include "ACFAIController.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFAIRoutineComponent.h"
#include "Components/MiningSiteComponent.h"
#include "Components/MiningSitePresentationCoordinatorComponent.h"
#include "Components/PangeaMiningConvoyComponent.h"
#include "Components/PangeaMiningTransportComponent.h"
#include "Data/ACFAIRoutineDataAsset.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "DataAssets/MiningSitePresentationConfig.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tasks/AITask_MoveTo.h"
#include "Tasks/PangeaMiningTransportRoutineTask.h"

UPangeaMiningTransportJobComponent::UPangeaMiningTransportJobComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = .1f;
}

bool UPangeaMiningTransportJobComponent::Configure(UPangeaMiningConvoyComponent* InConvoy, AACFCharacter* Pawn, int32 Slot)
{
	auto* AI = Cast<AACFAIController>(GetOwner());
	auto* Transport = IsValid(InConvoy) ? InConvoy->GetTransport() : nullptr;
	auto* Schedule = AI ? AI->FindComponentByClass<UACFAIRoutineComponent>() : nullptr;
	if (RuntimeRoutine || !AI || !AI->HasAuthority() || !IsValid(Pawn) || AI->GetPawn() != Pawn ||
		!Transport || !Transport->HasActiveShipment() || Transport->IsAwaitingRestore() ||
		!Schedule || !AI->GetBehaviorTree() || AI->GetOwner() != InConvoy->GetOwner()) return false;
	const FPangeaMiningShipment Trip = Transport->GetShipment();
	const bool bDraft = Slot == UPangeaMiningConvoyComponent::DraftAnimalSlot;
	if (bDraft ? Trip.DraftAnimalClass.IsNull() :
		(Slot < INDEX_NONE || Slot >= Trip.EscortCount || Trip.LostEscortSlots.Contains(Slot))) return false;
	Convoy = InConvoy;
	Character = Pawn;
	ManifestId = Trip.Id;
	EscortSlot = Slot;
	Routine = Schedule;
	OriginalRoutine = Schedule->GetRoutineDataAsset();
	if (const auto* Site = InConvoy->GetOwner()->FindComponentByClass<UMiningSiteComponent>(); Site && Site->SiteDefinition)
		if (const auto* Config = Cast<UMiningSitePresentationConfig>(Site->SiteDefinition->PresentationConfig.LoadSynchronous()))
		{
			NavigationTimeout = FMath::IsFinite(Config->NavigationTimeoutSeconds) ? FMath::Max(1.f, Config->NavigationTimeoutSeconds) : 120.f;
			RetryDelay = FMath::IsFinite(Config->BlockedRetryDelaySeconds) ? FMath::Max(.1f, Config->BlockedRetryDelaySeconds) : 3.f;
		}
	RuntimeRoutine = NewObject<UACFAIRoutineDataAsset>(this, NAME_None, RF_Transient);
	FAIRoutineTask Entry;
	Entry.TaskTime = FRoutineTime::FromMinutes(0);
	Entry.RoutineTask = NewObject<UPangeaMiningTransportRoutineTask>(RuntimeRoutine, NAME_None, RF_Transient);
	RuntimeRoutine->DailyTasks.Add(Entry);
	Schedule->StopAll();
	Schedule->SetRoutineDataAsset(RuntimeRoutine);
	AI->OnAIStateChanged.AddUniqueDynamic(this, &ThisClass::HandleAIStateChanged);
	// Never attach the humanoid presentation agent (or its montages) to a dinosaur.
	if (auto* Coordinator = InConvoy->GetOwner()->FindComponentByClass<UMiningSitePresentationCoordinatorComponent>(); Coordinator && !bDraft)
		Coordinator->ApplyRoutineTravelSpeed(Pawn, Slot == INDEX_NONE ? EMiningPresentationRole::Courier : EMiningPresentationRole::Guard,
			Trip.Speed * (Slot == INDEX_NONE ? 1.f : 1.1f));
	return true;
}

bool UPangeaMiningTransportJobComponent::CanExecute() const
{
	const auto* AI = Cast<AACFAIController>(GetOwner());
	const auto* Transport = Convoy.IsValid() ? Convoy->GetTransport() : nullptr;
	return AI && AI->HasAuthority() && Character.IsValid() && AI->GetPawn() == Character.Get() && Character->IsAlive() &&
		Transport && Transport->IsHybridEnabled() && !Transport->IsAwaitingRestore() && Transport->HasActiveShipment() && Transport->GetShipment().Id == ManifestId &&
		Routine.IsValid() && Routine->GetRoutineDataAsset() == RuntimeRoutine && !AI->IsExecutingCommand() &&
		AI->GetAIState().MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("AIState.Routine")));
}

void UPangeaMiningTransportJobComponent::BeginRoutineJob(UACFTask* Task)
{
	if (RuntimeRoutine && !RuntimeRoutine->DailyTasks.IsEmpty() && RuntimeRoutine->DailyTasks[0].RoutineTask == Task) RequestedTask = Task;
}
void UPangeaMiningTransportJobComponent::EndRoutineJob(UACFTask* Task)
{
	if (RequestedTask != Task) return;
	RequestedTask = nullptr;
	CancelOperation();
}
void UPangeaMiningTransportJobComponent::HandleAIStateChanged(FGameplayTag State)
{
	if (!CanExecute()) CancelOperation(); // Never stop the BT or someone else's MoveTo.
}
void UPangeaMiningTransportJobComponent::Present(EMiningPresentationState State, const FVector& Focus)
{
	if (EscortSlot == UPangeaMiningConvoyComponent::DraftAnimalSlot)
	{
		UCharacterMovementComponent* Movement = Character.IsValid() ? Character->GetCharacterMovement() : nullptr;
		if (!Movement) return;
		if (State == EMiningPresentationState::Traveling && CanExecute())
		{
			if (!bOwnsDraftSpeed) DraftOriginalSpeed = Movement->MaxWalkSpeed;
			DraftAppliedSpeed = Convoy->GetTransport()->GetShipment().Speed * 1.1f;
			Movement->MaxWalkSpeed = DraftAppliedSpeed;
			bOwnsDraftSpeed = true;
		}
		else
		{
			if (bOwnsDraftSpeed && FMath::IsNearlyEqual(Movement->MaxWalkSpeed, DraftAppliedSpeed))
				Movement->MaxWalkSpeed = DraftOriginalSpeed;
			bOwnsDraftSpeed = false;
		}
		return;
	}
	if (Convoy.IsValid() && Character.IsValid())
		if (auto* Coordinator = Convoy->GetOwner()->FindComponentByClass<UMiningSitePresentationCoordinatorComponent>())
			Coordinator->ApplyRoutinePresentation(Character.Get(),
				EscortSlot == INDEX_NONE ? EMiningPresentationRole::Courier : EMiningPresentationRole::Guard, State, Focus);
}
void UPangeaMiningTransportJobComponent::CancelOperation()
{
	UAITask_MoveTo* Pending = MoveTask;
	MoveTask = nullptr;
	if (Pending) Pending->OnMoveTaskFinished.RemoveAll(this);
	if (IsValid(Pending) && !Pending->IsFinished()) Pending->ExternalCancel();
	bMoveFinished = bMoveSucceeded = bHolding = false;
	Present(EMiningPresentationState::Idle, Character.IsValid() ? Character->GetActorLocation() : FVector::ZeroVector);
}
void UPangeaMiningTransportJobComponent::Block(const TCHAR* Reason)
{
	CancelOperation();
	BlockedReason = Reason;
	RetryAt = GetWorld()->GetTimeSeconds() + RetryDelay;
}
void UPangeaMiningTransportJobComponent::BeginHold(EMiningPresentationState State, const FVector& Focus)
{
	float Duration = 1.f;
	if (auto* Coordinator = Convoy->GetOwner()->FindComponentByClass<UMiningSitePresentationCoordinatorComponent>())
		Duration = Coordinator->GetRoutinePresentationDuration(Character.Get(), State, Duration);
	bHolding = true;
	HoldState = State;
	Deadline = GetWorld()->GetTimeSeconds() + FMath::Max(.1f, Duration);
	Present(State, Focus);
}

void UPangeaMiningTransportJobComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!RuntimeRoutine) return;
	if (!CanExecute())
	{
		if (MoveTask || bHolding) CancelOperation();
		return;
	}
	Routine->UpdateByTime(); // ACF, not this component, starts/ends the schedule entry.
	if (!RequestedTask) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < RetryAt) return;
	auto* Transport = Convoy->GetTransport();
	const FPangeaMiningShipment Trip = Transport->GetShipment();
	const bool bEscort = EscortSlot != INDEX_NONE;
	const FVector Goal = Trip.Phase == EPangeaMiningShipmentPhase::Returning ? Trip.Origin : Trip.Destination;
	AACFCharacter* Lead = Convoy->GetCourier();
	if (bEscort && (!IsValid(Lead) || !Lead->IsAlive())) { CancelOperation(); return; }
	if (bHolding)
	{
		if (Now < Deadline) return;
		const bool bUnload = HoldState == EMiningPresentationState::Unloading;
		bHolding = false;
		if (bUnload)
		{
			if (!Transport->CompletePhysicalLeg(Character.Get(), ManifestId)) { Block(TEXT("Physical arrival no longer matches manifest/endpoint")); return; }
			// A full/missing destination retains cargo. Wait before another complete
			// unload animation/attempt; retries can never manufacture more items.
			RetryAt = Now + RetryDelay;
		}
		else bLoaded = true;
		Present(EMiningPresentationState::Idle, Goal);
		return;
	}
	if (bMoveFinished || (MoveTask && MoveTask->IsFinished()))
	{
		const bool bSuccess = bMoveFinished ? bMoveSucceeded : MoveTask->WasMoveSuccessful();
		if (MoveTask) MoveTask->OnMoveTaskFinished.RemoveAll(this);
		MoveTask = nullptr;
		bMoveFinished = false;
		if (!bSuccess) { Block(TEXT("No complete convoy navigation path")); return; }
	}
	if (MoveTask)
	{
		if (Now >= Deadline) Block(TEXT("Convoy path-following timed out; no teleport or invisible delivery"));
		else if (!FMath::IsNearlyEqual(Character->GetCharacterMovement()->MaxWalkSpeed, Trip.Speed * (bEscort ? 1.1f : 1.f)))
		{
			// ACF locomotion initialization can finish after our first travel frame.
			// Reapply only while THIS requested routine owns an active MoveTo; never
			// force duty speed during combat, commands, holds or another schedule.
			if (EscortSlot == UPangeaMiningConvoyComponent::DraftAnimalSlot)
				Present(EMiningPresentationState::Traveling, Lead->GetActorLocation());
			else if (auto* Coordinator = Convoy->GetOwner()->FindComponentByClass<UMiningSitePresentationCoordinatorComponent>())
				Coordinator->ApplyRoutineTravelSpeed(Character.Get(), bEscort ? EMiningPresentationRole::Guard : EMiningPresentationRole::Courier,
					Trip.Speed * (bEscort ? 1.1f : 1.f));
		}
		return;
	}
	if (!bEscort && !bLoaded && Trip.Phase == EPangeaMiningShipmentPhase::Outbound &&
		FVector::DistSquared(Character->GetActorLocation(), Trip.Origin) <= FMath::Square(250.))
	{
		BeginHold(EMiningPresentationState::Loading, Goal);
		return;
	}
	const FVector Target = bEscort ? Lead->GetActorLocation() : Goal;
	const double Radius = bEscort ? 300. : 180.;
	if (FVector::DistSquared(Character->GetActorLocation(), Target) <= Radius * Radius)
	{
		if (bEscort) Present(EMiningPresentationState::Guarding, Lead->GetActorLocation());
		else if (!Convoy->IsDraftReadyForArrival(Goal))
		{
			// No teleport, unload animation, or virtual fallback while the cart's
			// animal is blocked, fighting, commanded or still catching up.
			Present(EMiningPresentationState::Idle, Goal);
			BlockedReason = TEXT("Waiting for draft animal and cart at endpoint");
			RetryAt = Now + RetryDelay;
		}
		else if (Trip.Phase == EPangeaMiningShipmentPhase::Returning)
		{
			if (!Transport->CompletePhysicalLeg(Character.Get(), ManifestId)) Block(TEXT("Return endpoint rejected"));
		}
		else BeginHold(EMiningPresentationState::Unloading, Goal);
		return;
	}
	FAIMoveRequest Request;
	if (bEscort) Request.SetGoalActor(Lead); else Request.SetGoalLocation(Goal);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(false);
	Request.SetAcceptanceRadius(bEscort ? 200.f : 60.f);
	Request.SetReachTestIncludesAgentRadius(true);
	auto* AI = CastChecked<AACFAIController>(GetOwner());
	MoveTask = UAITask::NewAITask<UAITask_MoveTo>(*AI, EAITaskPriority::High);
	if (!MoveTask) { Block(TEXT("Cannot allocate owned convoy move task")); return; }
	MoveTask->SetUp(AI, Request);
	MoveTask->OnMoveTaskFinished.AddWeakLambda(this, [this](TEnumAsByte<EPathFollowingResult::Type> Result, AAIController*)
	{
		bMoveSucceeded = Result == EPathFollowingResult::Success;
		bMoveFinished = true;
	});
	BlockedReason.Reset();
	Deadline = Now + NavigationTimeout;
	Present(EMiningPresentationState::Traveling, Target);
	MoveTask->ReadyForActivation();
}

void UPangeaMiningTransportJobComponent::ReleaseAssignment()
{
	if (auto* AI = Cast<AACFAIController>(GetOwner())) AI->OnAIStateChanged.RemoveDynamic(this, &ThisClass::HandleAIStateChanged);
	RequestedTask = nullptr;
	CancelOperation();
	if (Routine.IsValid() && RuntimeRoutine && Routine->GetRoutineDataAsset() == RuntimeRoutine)
	{
		Routine->StopAll();
		Routine->SetRoutineDataAsset(OriginalRoutine);
	}
	Routine.Reset();
	RuntimeRoutine = OriginalRoutine = nullptr;
	Character.Reset();
	Convoy.Reset();
	ManifestId.Invalidate();
}
void UPangeaMiningTransportJobComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseAssignment();
	Super::EndPlay(Reason);
}
