#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ALSFunctionLibrary.h"
#include "ALSSaveTypes.h"
#include "ACFAIController.h"
#include "Animation/ACFAnimInstance.h"
#include "Animation/ACFMovesetLayer.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Actors/ACFCharacter.h"
#include "Actors/MiningDiscoveryNodeActor.h"
#include "Actors/MiningSettlementStockpileActor.h"
#include "Actors/MiningSiteActor.h"
#include "Commands/ACFFollowMeCommand.h"
#include "Components/ACFCommandsManagerComponent.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Components/ACFSplineFollowerComponent.h"
#include "Components/MiningPresentationAgentComponent.h"
#include "Components/MiningRoutineJobComponent.h"
#include "Components/MiningSiteComponent.h"
#include "Components/MiningSitePresentationCoordinatorComponent.h"
#include "Components/PangeaMiningTransportComponent.h"
#include "Components/PangeaMiningConvoyComponent.h"
#include "Components/PangeaMiningTransportJobComponent.h"
#include "Components/ACFStorageComponent.h"
#include "Components/PangeaMiningTransportStorageComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GameplayTask.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "Navigation/PathFollowingComponent.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
UWorld* MiningPIEWorld()
{
	if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetFirstPlayerController()) return Context.World();
	return nullptr;
}

template<class T> T* MiningObject(UObject* Owner, const FName Name)
{
	const FObjectPropertyBase* Property = Owner ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name) : nullptr;
	return Property ? Cast<T>(Property->GetObjectPropertyValue_InContainer(Owner)) : nullptr;
}

AActor* MiningWorker(AMiningSiteActor* Site, int32 Index)
{
	const FArrayProperty* Property = Site ? FindFProperty<FArrayProperty>(Site->GetClass(), TEXT("SpawnedWorkerActors")) : nullptr;
	if (!Property) return nullptr;
	FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Site));
	return Index < Array.Num() ? Cast<AActor>(CastFieldChecked<FObjectPropertyBase>(Property->Inner)->GetObjectPropertyValue(Array.GetRawPtr(Index))) : nullptr;
}

struct FMiningPIEProbe
{
	FAutomationTestBase& Test;
	TWeakObjectPtr<AMiningSiteActor> Site;
	TWeakObjectPtr<AACFCharacter> Worker;
	TWeakObjectPtr<AACFAIController> Controller;
	TWeakObjectPtr<UMiningRoutineJobComponent> Job;
	TWeakObjectPtr<USmartObjectComponent> Station;
	TWeakObjectPtr<UACFFollowMeCommand> Command;
	FVector Start = FVector::ZeroVector;
	FVector BlockedStart = FVector::ZeroVector;
	int32 Phase = 0;
	double Deadline = FPlatformTime::Seconds() + 30.0;
	double ReadyAt = 0.0;
	bool bWorked = false;
	bool bDeposited = false;
	bool bGuarded = false;
	bool bLoaded = false;
	bool bUnloaded = false;
	bool bTransportFixture = false;
	explicit FMiningPIEProbe(FAutomationTestBase& InTest) : Test(InTest) {}

	void Next(int32 InPhase, double Seconds)
	{
		Phase = InPhase;
		Deadline = FPlatformTime::Seconds() + Seconds;
		Test.AddInfo(FString::Printf(TEXT("Mining PIE phase %d"), Phase));
	}
	bool Fail(const FString& Message)
	{
		Test.AddError(Message);
		Cleanup();
		return true;
	}
	void Cleanup()
	{
		if (Command.IsValid()) Command->FinishExecution(false);
		if (Site.IsValid()) Site->Destroy();
		Site.Reset();
	}
	bool ClaimsFree(UWorld* World)
	{
		USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World);
		if (!Subsystem || !Station.IsValid()) return false;
		TArray<FSmartObjectSlotHandle> Slots;
		Subsystem->GetAllSlots(Station->GetRegisteredHandle(), Slots);
		return Slots.Num() >= 2 && Subsystem->GetSlotState(Slots[0]) == ESmartObjectSlotState::Free && Subsystem->GetSlotState(Slots[1]) == ESmartObjectSlotState::Free;
	}
	bool Tick()
	{
		UWorld* World = MiningPIEWorld();
		if (FPlatformTime::Seconds() > Deadline)
		{
			return Fail(FString::Printf(TEXT("Mining PIE phase %d timed out. Worker=%s Alive=%d Position=%s Job=%d Reason=%s Worked=%d Deposit=%d Guard=%d Load=%d Unload=%d"), Phase,
				*GetNameSafe(Worker.Get()), Worker.IsValid() && Worker->IsAlive(), Worker.IsValid() ? *Worker->GetActorLocation().ToString() : TEXT("none"),
				Job.IsValid() ? static_cast<int32>(Job->GetJobStatus()) : -1, Job.IsValid() ? *Job->GetBlockedReason() : TEXT("none"), bWorked, bDeposited, bGuarded, bLoaded, bUnloaded));
		}
		if (!World) return false;
		if (Phase == 0)
		{
			if (ReadyAt == 0.0) { ReadyAt = FPlatformTime::Seconds() + 2.0; return false; }
			if (FPlatformTime::Seconds() < ReadyAt) return false;
			AMiningDiscoveryNodeActor* Node = nullptr;
			for (TActorIterator<AMiningDiscoveryNodeActor> It(World); It; ++It)
				if (It->ActorHasTag(TEXT("Pangea.Mining.Prototype")) == bTransportFixture) { Node = *It; break; }
			if (!Node || !Node->SiteDefinition || !Node->MiningSiteActorClass) return Fail(TEXT("Current PIE map needs the configured mining discovery node"));
			const UMiningSiteDefinition* SourceDefinition = bTransportFixture
				? LoadObject<UMiningSiteDefinition>(nullptr, TEXT("/Game/Yarin/DA_Pangea_MiningSite_HybridPrototype.DA_Pangea_MiningSite_HybridPrototype")) : Node->SiteDefinition.Get();
			if (!SourceDefinition) return Fail(TEXT("Mining prototype definition missing"));
			UMiningSiteDefinition* Definition = DuplicateObject<UMiningSiteDefinition>(SourceDefinition, GetTransientPackage());
			FMiningSiteLevelDefinition Level;
			const UMiningSitePresentationConfig* Config = Cast<UMiningSitePresentationConfig>(Definition->PresentationConfig.LoadSynchronous());
			if (!Definition->GetLevelDefinition(bTransportFixture ? 5 : 3, Level) || !Config || !Config->bUseACFRoutineJobs) return Fail(TEXT("Configured mining tier and ACF routine jobs required"));
			// Test-only tier fixture: no cost/progression or content asset is edited or saved.
			Level.Level = 0;
			if (bTransportFixture)
			{
				Definition->bUseHybridTransport = true;
				Level.AutomatedMineralsPerDay = 0;
				Level.MaxShipmentUnits = 5;
				Level.ShipmentIntervalSeconds = 0.f;
				Level.ShipmentLossChance = 0.f;
				// Keep the prototype's real courier, cart, escort and speed settings.
			}
			Definition->Levels = { Level };
			const FTransform Transform = Node->SiteSpawnTransform * Node->GetActorTransform();
			APawn* Player = World->GetFirstPlayerController()->GetPawn();
			if (!Player) return Fail(TEXT("PIE player required for presentation relevance"));
			Player->SetActorLocation(Transform.GetLocation() + FVector(-600, -600, 100), false, nullptr, ETeleportType::TeleportPhysics);
			if (bTransportFixture) UNavigationSystemV1::RegisterNavigationInvoker(*Player, 5000.f, 5500.f);
			UClass* SiteClass = bTransportFixture
				? LoadClass<AMiningSiteActor>(nullptr, TEXT("/Game/Yarin/BP_Pangea_MiningSite_HybridPrototype.BP_Pangea_MiningSite_HybridPrototype_C")) : Node->MiningSiteActorClass.Get();
			if (!SiteClass) return Fail(TEXT("Mining prototype site class missing"));
			AMiningSiteActor* Fixture = World->SpawnActorDeferred<AMiningSiteActor>(SiteClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Fixture) return Fail(TEXT("Cannot create mining-site PIE fixture"));
			Site = Fixture;
			Fixture->SetFlags(RF_Transient);
			Fixture->MiningSiteComponent->SiteDefinition = Definition;
			Fixture->SettlementResourceActor = Node->SettlementResourceActor;
			Fixture->FinishSpawning(Transform);
			if (!Fixture->MiningSiteComponent->EstablishSite()) return Fail(TEXT("Cannot establish tier fixture"));
			Next(1, 15.0);
			return false;
		}
		if (!Site.IsValid()) return Fail(TEXT("Mining fixture disappeared"));
		if (Phase == 1)
		{
			Worker = Cast<AACFCharacter>(MiningWorker(Site.Get(), 0));
			if (!Worker.IsValid()) return false;
			Controller = Cast<AACFAIController>(Worker->GetController());
			Job = Controller.IsValid() ? Controller->FindComponentByClass<UMiningRoutineJobComponent>() : nullptr;
			if (!Job.IsValid()) return false;
			if (Controller->GetOwner() == Worker.Get() || Worker->GetOwner() != Controller.Get()) return Fail(TEXT("Mining controller/pawn ownership is not an acyclic possession chain"));
			TInlineComponentArray<USmartObjectComponent*> Stations(Site.Get());
			for (USmartObjectComponent* Candidate : Stations)
				if (Candidate->GetName() == TEXT("WorkerSmartObjectComponent_1")) Station = Candidate;
			if (!Station.IsValid()) return Fail(TEXT("Worker station 1 missing"));
			UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			FNavLocation Point;
			if (!Nav || !Nav->ProjectPointToNavigation(Worker->GetActorLocation() + FVector(-450, 0, 0), Point, FVector(250, 250, 300))) return Fail(TEXT("No usable generated navigation near the mining worker spawn"));
			Controller->SetCurrentAIState(FGameplayTag::RequestGameplayTag(TEXT("AIState.Wait")));
			Worker->SetActorLocation(Point.Location + FVector(0, 0, Worker->GetSimpleCollisionHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
			Start = Worker->GetActorLocation(); // Fixture arrangement only; mining movement afterward is never teleported.
			Controller->ResetToDefaultState();
			Next(2, 45.0);
			return false;
		}
		if (!Worker.IsValid() || !Controller.IsValid() || !Job.IsValid()) return Fail(TEXT("Assigned worker/controller/job lost"));
		if (Phase == 2)
		{
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				if (!It->ActorHasTag(FName(*FString::Printf(TEXT("Mining.Site.%s"), *Site->GetName())))) continue;
				UMiningPresentationAgentComponent* Agent = It->FindComponentByClass<UMiningPresentationAgentComponent>();
				if (!Agent) continue;
				if (*It == Worker.Get())
				{
					if (Agent->GetCurrentState() == EMiningPresentationState::Working)
					{
						USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World);
						TArray<FSmartObjectSlotHandle> Slots;
						Subsystem->GetAllSlots(Station->GetRegisteredHandle(), Slots);
						const TOptional<FVector> Location = Slots.IsEmpty() ? TOptional<FVector>() : Subsystem->GetSlotLocation(Slots[0]);
						if (!Location.IsSet() || FVector::Dist2D(Worker->GetActorLocation(), Location.GetValue()) > 150.0f) return Fail(TEXT("Working state reported before reaching its Smart Object slot"));
					}
					bWorked |= Agent->GetCurrentState() == EMiningPresentationState::Working;
					bDeposited |= Agent->GetCurrentState() == EMiningPresentationState::Depositing;
				}
				bGuarded |= Agent->GetCurrentState() == EMiningPresentationState::Guarding;
				bLoaded |= Agent->GetCurrentState() == EMiningPresentationState::Loading;
				bUnloaded |= Agent->GetCurrentState() == EMiningPresentationState::Unloading;
			}
			if (!bWorked || !bDeposited || !bGuarded || !bLoaded || !bUnloaded || Job->GetJobStatus() != EMiningRoutineJobStatus::Interacting) return false;
			if (FVector::Dist2D(Start, Worker->GetActorLocation()) < 100.0f) return Fail(TEXT("Presentation states changed without meaningful worker navigation"));
			Test.AddInfo(TEXT("Observed worker work/deposit, guard post and courier load/unload after path-following movement"));
			Controller->SetCurrentAIState(FGameplayTag::RequestGameplayTag(TEXT("AIState.Combat")));
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Suspended || MiningObject<UGameplayTask>(Job.Get(), TEXT("InteractionTask")) || !ClaimsFree(World)) return Fail(TEXT("Combat-state transition did not immediately cancel duty/release claims"));
			Controller->ResetToDefaultState();
			Next(3, 20.0);
			return false;
		}
		if (Phase == 3)
		{
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Interacting) return false;
			// A real ACF command manager + spline command. Existing tag is only an isolated fixture dispatch key.
			UACFCommandsManagerComponent* Manager = Controller->FindComponentByClass<UACFCommandsManagerComponent>();
			const FGameplayTag Key = FGameplayTag::RequestGameplayTag(TEXT("AIState.Wait"));
			if (!Manager || !Manager->AddCommand(Key, UACFFollowMeCommand::StaticClass())) return Fail(TEXT("Cannot install fixture ACF command"));
			const FMapProperty* Map = FindFProperty<FMapProperty>(Manager->GetClass(), TEXT("commands"));
			if (!Map) return Fail(TEXT("ACF commands map unavailable"));
			FScriptMapHelper Entries(Map, Map->ContainerPtrToValuePtr<void>(Manager));
			for (int32 I = 0; I < Entries.GetMaxIndex(); ++I)
				if (Entries.IsValidIndex(I) && *reinterpret_cast<FGameplayTag*>(Entries.GetKeyPtr(I)) == Key)
					Command = Cast<UACFFollowMeCommand>(CastFieldChecked<FObjectPropertyBase>(Map->ValueProp)->GetObjectPropertyValue(Entries.GetValuePtr(I)));
			if (!Command.IsValid()) return Fail(TEXT("Fixture spline command missing"));
			USplineComponent* Spline = NewObject<USplineComponent>(Site.Get());
			Site->AddInstanceComponent(Spline);
			Spline->RegisterComponent();
			Spline->ClearSplinePoints(false);
			// Do not start the spline exactly under the pawn. The stock follower can
			// synchronously complete an already-at-goal point before this assertion.
			UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			FVector Goal = FVector::ZeroVector;
			bool bFoundGoal = false;
			for (const FVector& Offset : { FVector(900, 0, 0), FVector(0, 900, 0), FVector(-900, 0, 0), FVector(0, -900, 0) })
			{
				FNavLocation Point;
				if (!Nav || !Nav->ProjectPointToNavigation(Worker->GetActorLocation() + Offset, Point, FVector(150, 150, 300))) continue;
				UNavigationPath* Path = Nav->FindPathToLocationSynchronously(World, Worker->GetActorLocation(), Point.Location, Worker.Get());
				if (FVector::Dist2D(Worker->GetActorLocation(), Point.Location) < 600 || !Path || !Path->IsValid() || Path->IsPartial()) continue;
				Goal = Point.Location;
				bFoundGoal = true;
				break;
			}
			if (!bFoundGoal) return Fail(TEXT("Cannot arrange a nontrivial reachable ACF command destination"));
			Spline->AddSplinePoint(Goal, ESplineCoordinateSpace::World, false);
			Spline->AddSplinePoint(Goal + FVector(0, 500, 0), ESplineCoordinateSpace::World, true);
			Command->SetupCommand(Spline, World->GetFirstPlayerController()->GetPawn());
			Manager->TriggerCommand(Key);
			Manager->ExecutePendingCommand();
			ReadyAt = FPlatformTime::Seconds() + 0.4;
			Next(4, 5.0);
			return false;
		}
		if (Phase == 4)
		{
			if (FPlatformTime::Seconds() < ReadyAt) return false;
			if (!Controller->IsExecutingCommand() || Job->GetJobStatus() != EMiningRoutineJobStatus::Suspended || !ClaimsFree(World)) return Fail(FString::Printf(TEXT("ACF command interruption failed. Executing=%d Job=%d ClaimsFree=%d"), Controller->IsExecutingCommand(), static_cast<int32>(Job->GetJobStatus()), ClaimsFree(World)));
			Controller->FindComponentByClass<UACFCommandsManagerComponent>()->TerminateCurrentCommand();
			Command->FinishExecution(true);
			Command.Reset();
			Controller->ResetToDefaultState();
			Next(5, 20.0);
			return false;
		}
		if (Phase == 5)
		{
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Interacting) return false;
			World->GetTimerManager().ClearAllTimersForObject(Site.Get()); // Freeze site reassignment for this isolated failure injection.
			Job->ReleaseAssignment();
			UMiningSitePresentationCoordinatorComponent* Coordinator = Cast<UMiningSitePresentationCoordinatorComponent>(Site->PresentationCoordinator);
			const FVector Unreachable(10000000, 10000000, 1000000);
			if (!Job->ConfigureAssignment(Coordinator, Worker.Get(), EMiningPresentationRole::Courier, nullptr, Unreachable, Unreachable, 0.2f, 3.0f, 3.0f)) return Fail(TEXT("Cannot arrange no-path duty"));
			BlockedStart = Worker->GetActorLocation();
			Next(6, 6.0);
			return false;
		}
		if (Phase == 6)
		{
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Blocked) return false;
			if (FVector::Dist2D(BlockedStart, Worker->GetActorLocation()) > 50.0f) return Fail(TEXT("Unreachable duty moved the pawn instead of failing closed"));
			Test.AddInfo(TEXT("Unreachable navigation blocks without teleport/direct movement; combat and real command interruption/resumption passed"));
			Cleanup();
			if (Job.IsValid() && Job->GetJobStatus() != EMiningRoutineJobStatus::Unassigned) return Fail(TEXT("Site teardown left an assigned controller job"));
			return true;
		}
		return false;
	}
};

/** Reuses the same transient site setup; exercises failure/recovery rather than happy-path loops. */
struct FMiningLifecyclePIEProbe : FMiningPIEProbe
{
	FSmartObjectClaimHandle OtherClaim;
	TArray<FSmartObjectSlotHandle> Slots;
	TWeakObjectPtr<AACFCharacter> RetiredWorker;
	TWeakObjectPtr<AACFAIController> RetiredController;
	TWeakObjectPtr<UMiningRoutineJobComponent> RetiredJob;
	FVector BusyStart = FVector::ZeroVector;
	explicit FMiningLifecyclePIEProbe(FAutomationTestBase& InTest) : FMiningPIEProbe(InTest) {}

	bool Tick()
	{
		if (Phase < 2) return FMiningPIEProbe::Tick();
		UWorld* World = MiningPIEWorld();
		if (!World || !Site.IsValid()) return Fail(TEXT("Lifecycle PIE world/site disappeared"));
		if (FPlatformTime::Seconds() > Deadline)
			return Fail(FString::Printf(TEXT("Lifecycle phase %d timed out. Worker=%s Controller=%s Job=%d Reason=%s"), Phase,
				*GetNameSafe(Worker.Get()), *GetNameSafe(Controller.Get()), Job.IsValid() ? static_cast<int32>(Job->GetJobStatus()) : -1,
				Job.IsValid() ? *Job->GetBlockedReason() : TEXT("none")));
		USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World);
		if (!Subsystem || !Station.IsValid()) return Fail(TEXT("Lifecycle station/subsystem missing"));
		if (Phase == 10)
		{
			if (IsValid(MiningWorker(Site.Get(), 0))) return false;
			if (RetiredWorker.IsValid() || (RetiredJob.IsValid() && RetiredJob->GetJobStatus() != EMiningRoutineJobStatus::Unassigned) || !ClaimsFree(World)) return Fail(TEXT("Distance culling stranded a pawn, assigned job or workstation claim"));
			for (TActorIterator<AACFAIController> It(World); It; ++It)
				if (It->GetOwner() == Site.Get() && !It->GetPawn() && !It->IsActorBeingDestroyed()) return Fail(TEXT("Distance culling left an orphaned mining controller"));
			APawn* Player = World->GetFirstPlayerController()->GetPawn();
			if (!Player) return Fail(TEXT("Player missing during relevance recovery"));
			Player->SetActorLocation(Site->GetActorLocation() + FVector(-600, -600, 100), false, nullptr, ETeleportType::TeleportPhysics);
			Next(11, 20.0);
			return false;
		}
		if (Phase == 11)
		{
			AACFCharacter* NewWorker = Cast<AACFCharacter>(MiningWorker(Site.Get(), 0));
			if (!IsValid(NewWorker)) return false;
			if (TWeakObjectPtr<AACFCharacter>(NewWorker) == RetiredWorker) return Fail(TEXT("Relevance recovery reused the culled actor"));
			AACFAIController* NewController = Cast<AACFAIController>(NewWorker->GetController());
			UMiningRoutineJobComponent* NewJob = NewController ? NewController->FindComponentByClass<UMiningRoutineJobComponent>() : nullptr;
			if (!NewJob || NewJob->GetJobStatus() != EMiningRoutineJobStatus::Interacting) return false;
			Test.AddInfo(TEXT("Lifecycle acceptance: busy slot preserved, travel canceled, station removal/re-register recovered, unpossession/repossess recovered, distance culling/re-spawn resumed"));
			Cleanup();
			return true;
		}
		if (!Worker.IsValid() || !Controller.IsValid() || !Job.IsValid()) return Fail(TEXT("Lifecycle worker/controller/job unexpectedly lost"));
		if (Phase == 2)
		{
			Controller->SetCurrentAIState(FGameplayTag::RequestGameplayTag(TEXT("AIState.Wait")));
			// Reset the private duty to primary phase; the site can keep refreshing this same assignment.
			Job->ReleaseAssignment();
			UMiningSitePresentationCoordinatorComponent* Coordinator = Cast<UMiningSitePresentationCoordinatorComponent>(Site->PresentationCoordinator);
			Subsystem->GetAllSlots(Station->GetRegisteredHandle(), Slots);
			if (Slots.Num() < 2) return Fail(TEXT("Lifecycle station needs work and deposit slots"));
			const TOptional<FVector> Primary = Subsystem->GetSlotLocation(Slots[0]);
			const TOptional<FVector> Secondary = Subsystem->GetSlotLocation(Slots[1]);
			if (!Primary.IsSet() || !Secondary.IsSet() || !Job->ConfigureAssignment(Coordinator, Worker.Get(), EMiningPresentationRole::Worker, Station.Get(), Primary.GetValue(), Secondary.GetValue(), 2.0f)) return Fail(TEXT("Cannot arrange busy-slot assignment"));
			OtherClaim = Subsystem->MarkSlotAsClaimed(Slots[0], ESmartObjectClaimPriority::Normal);
			if (!OtherClaim.IsValid()) return Fail(TEXT("Cannot reserve station for the competing user"));
			BusyStart = Worker->GetActorLocation();
			Controller->ResetToDefaultState();
			ReadyAt = FPlatformTime::Seconds() + 0.4;
			Next(3, 6.0);
			return false;
		}
		if (Phase == 3)
		{
			if (FPlatformTime::Seconds() < ReadyAt) return false;
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Blocked || !Subsystem->IsClaimedSmartObjectValid(OtherClaim)
				|| Subsystem->GetSlotState(Slots[0]) != ESmartObjectSlotState::Claimed || Subsystem->GetSlotState(Slots[1]) != ESmartObjectSlotState::Free
				|| MiningObject<UGameplayTask>(Job.Get(), TEXT("InteractionTask")) || FVector::Dist2D(BusyStart, Worker->GetActorLocation()) > 30.0f)
				return Fail(TEXT("Busy primary slot was stolen, bypassed with another slot, or caused movement instead of backoff"));
			Subsystem->MarkSlotAsFree(OtherClaim);
			OtherClaim.Invalidate();
			Next(4, 12.0);
			return false;
		}
		if (Phase == 4)
		{
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Traveling || Controller->GetMoveStatus() != EPathFollowingStatus::Moving) return false;
			TWeakObjectPtr<UGameplayTask> MovingTask = MiningObject<UGameplayTask>(Job.Get(), TEXT("InteractionTask"));
			if (!MovingTask.IsValid()) return Fail(TEXT("Traveling duty has no owned task"));
			Controller->SetCurrentAIState(FGameplayTag::RequestGameplayTag(TEXT("AIState.Combat")));
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Suspended || (MovingTask.IsValid() && !MovingTask->IsFinished()) || !ClaimsFree(World)) return Fail(TEXT("Combat during travel did not cancel movement/task and release the reserved station"));
			Controller->ResetToDefaultState();
			Next(5, 15.0);
			return false;
		}
		if (Phase == 5)
		{
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Interacting) return false;
			TWeakObjectPtr<UGameplayTask> Active = MiningObject<UGameplayTask>(Job.Get(), TEXT("InteractionTask"));
			if (!Subsystem->RemoveSmartObject(Station.Get())) return Fail(TEXT("Cannot remove active station runtime"));
			if (Active.IsValid() && !Active->IsFinished()) return Fail(TEXT("Removed station did not invalidate/cancel its active task"));
			ReadyAt = FPlatformTime::Seconds() + 0.4;
			Next(6, 5.0);
			return false;
		}
		if (Phase == 6)
		{
			if (FPlatformTime::Seconds() < ReadyAt) return false;
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Blocked || MiningObject<UGameplayTask>(Job.Get(), TEXT("InteractionTask"))) return Fail(TEXT("Removed station did not leave the duty blocked without an interaction task"));
			if (!Subsystem->RegisterSmartObject(Station.Get())) return Fail(TEXT("Cannot re-register removed station"));
			Next(7, 15.0);
			return false;
		}
		if (Phase == 7)
		{
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Interacting) return false;
			Controller->UnPossess();
			ReadyAt = FPlatformTime::Seconds() + 0.4;
			Next(8, 5.0);
			return false;
		}
		if (Phase == 8)
		{
			if (FPlatformTime::Seconds() < ReadyAt) return false;
			if (Worker->GetController() || Job->GetJobStatus() != EMiningRoutineJobStatus::Suspended || !ClaimsFree(World)) return Fail(TEXT("Unpossession stranded an interaction/claim or the site stole possession back"));
			Controller->Possess(Worker.Get());
			Controller->ResetToDefaultState();
			Next(9, 15.0);
			return false;
		}
		if (Phase == 9)
		{
			if (Job->GetJobStatus() != EMiningRoutineJobStatus::Interacting) return false;
			RetiredWorker = Worker;
			RetiredController = Controller;
			RetiredJob = Job;
			APawn* Player = World->GetFirstPlayerController()->GetPawn();
			if (!Player) return Fail(TEXT("Player missing during culling setup"));
			Player->SetActorLocation(Site->GetActorLocation() + FVector(6000, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
			Next(10, 6.0);
			return false;
		}
		return false;
	}
};
}

namespace
{
/** Sample evaluated poses while actual path following is moving each configured role. */
struct FMiningAnimationPIEProbe : FMiningPIEProbe
{
	bool bArranged = false;
	bool bObserverPinned = false;
	bool bAnimated[3] = { false, false, false };
	bool bCruiseChecked[3] = { false, false, false };
	bool bStationAnimation[3] = { false, false, false };
	bool bWorkerRetrieval = false;
	bool bWorkerDeposit = false;
	TArray<FTransform> PreviousPose[3];
	explicit FMiningAnimationPIEProbe(FAutomationTestBase& InTest) : FMiningPIEProbe(InTest) {}
	bool Tick()
	{
		UWorld* World = MiningPIEWorld();
		if (!World) return Phase < 2 ? FMiningPIEProbe::Tick() : Fail(TEXT("Animation world disappeared"));
		// This case measures NPC animation with a nearby observer, not player startup
		// movement or relevance culling. A pending player action/load can otherwise
		// move the observer back to spawn and correctly despawn every role mid-test.
		APlayerController* ObserverController = World->GetFirstPlayerController();
		AACFCharacter* Observer = ObserverController ? Cast<AACFCharacter>(ObserverController->GetPawn()) : nullptr;
		if (!Observer) return Phase < 2 ? FMiningPIEProbe::Tick() : Fail(TEXT("Animation observer missing"));
		// Arrange the observer BEFORE the base fixture's warmup/teleport can cause
		// fall damage. Project-player death opens a pausing menu: pinning a dead
		// observer at phase 2 was too late and froze every otherwise valid NPC job.
		// NPC health, movement, tasks and montage assertions are not altered.
		Observer->SetCanBeDamaged(false);
		Observer->SetIsImmortal(true);
		if (!bObserverPinned)
		{
			if (Observer->GetActionsComponent()) Observer->GetActionsComponent()->CancelAllAbilities();
			Observer->StopAnimMontage();
			Observer->GetCharacterMovement()->StopMovementImmediately();
			Observer->GetCharacterMovement()->DisableMovement();
			bObserverPinned = true;
		}
		if (Phase < 2) return FMiningPIEProbe::Tick();
		if (!Site.IsValid()) return Fail(TEXT("Animation fixture disappeared"));
		if (!Observer->IsAlive() || World->IsPaused())
			return Fail(FString::Printf(TEXT("Animation observer precondition failed: alive=%d paused=%d; NPC acceptance requires an unpaused world"), Observer->IsAlive(), World->IsPaused()));
		const FVector ObserverAnchor = Site->GetActorLocation() + FVector(-600, -600, 100);
		if (!Observer->GetActorLocation().Equals(ObserverAnchor, 1.f))
			Observer->SetActorLocation(ObserverAnchor, false, nullptr, ETeleportType::TeleportPhysics);
		if (FPlatformTime::Seconds() > Deadline)
		{
			const APlayerController* PC = World->GetFirstPlayerController();
			const AACFCharacter* Player = PC ? Cast<AACFCharacter>(PC->GetPawn()) : nullptr;
			Test.AddInfo(FString::Printf(TEXT("Animation fixture: arranged=%d net=%d player=%s alive=%d location=%s paused=%d gameTime=%.3f dilation=%.3f"), bArranged, static_cast<int32>(World->GetNetMode()), *GetNameSafe(Player), Player && Player->IsAlive(), Player ? *Player->GetActorLocation().ToString() : TEXT("none"), World->IsPaused(), World->GetTimeSeconds(), World->GetWorldSettings()->GetEffectiveTimeDilation()));
			const FName Tag(*FString::Printf(TEXT("Mining.Site.%s"), *Site->GetName()));
			for (TActorIterator<AACFCharacter> It(World); It; ++It)
			{
				if (!It->ActorHasTag(Tag)) continue;
				AACFAIController* AI = Cast<AACFAIController>(It->GetController());
				UMiningRoutineJobComponent* Duty = AI ? AI->FindComponentByClass<UMiningRoutineJobComponent>() : nullptr;
				UMiningPresentationAgentComponent* Agent = It->FindComponentByClass<UMiningPresentationAgentComponent>();
				Test.AddInfo(FString::Printf(TEXT("Animation role %s alive=%d location=%s state=%d duty=%d blocked=%s"), *It->GetName(), It->IsAlive(), *It->GetActorLocation().ToString(), Agent ? static_cast<int32>(Agent->GetCurrentState()) : -1, Duty ? static_cast<int32>(Duty->GetJobStatus()) : -1, Duty ? *Duty->GetBlockedReason() : TEXT("missing")));
			}
			return Fail(FString::Printf(TEXT("Animation acceptance timed out. Travel W/G/C=%d/%d/%d Station=%d/%d/%d Retrieve=%d Deposit=%d"), bAnimated[0], bAnimated[1], bAnimated[2], bStationAnimation[0], bStationAnimation[1], bStationAnimation[2], bWorkerRetrieval, bWorkerDeposit));
		}
		const FName SiteTag(*FString::Printf(TEXT("Mining.Site.%s"), *Site->GetName()));
		int32 ArrangedCount = 0;
		for (TActorIterator<AACFCharacter> It(World); It; ++It)
		{
			if (!It->ActorHasTag(SiteTag)) continue;
			UMiningPresentationAgentComponent* Agent = It->FindComponentByClass<UMiningPresentationAgentComponent>();
			AACFAIController* AI = Cast<AACFAIController>(It->GetController());
			if (!Agent || !AI) continue;
			const int32 Role = static_cast<int32>(Agent->GetCurrentRole());
			if (!bArranged)
			{
				FNavLocation Point;
				UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
				if (!Nav || !Nav->ProjectPointToNavigation(It->GetActorLocation() + FVector(-650, 0, 0), Point, FVector(250, 250, 300))) return Fail(TEXT("Cannot arrange role animation travel fixture"));
				AI->SetCurrentAIState(FGameplayTag::RequestGameplayTag(TEXT("AIState.Wait")));
				It->SetActorLocation(Point.Location + FVector(0, 0, It->GetSimpleCollisionHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
				AI->ResetToDefaultState();
				++ArrangedCount;
				continue;
			}
			UACFAnimInstance* Anim = It->GetACFAnimInstance();
			if (!Anim || !Anim->GetCurrentMoveset()) return Fail(FString::Printf(TEXT("%s has no active ACF locomotion moveset"), *It->GetName()));
			USkeletalMeshComponent* Mesh = It->GetMesh();
			// Changing poses alone also passes with the sample AnimBP's idle/jog blend.
			// Assert the actual spawned classes, not just transient mesh CDO edits: the
			// role construction scripts must retain this assignment across reloads.
			if (Anim->GetClass()->GetPathName() != TEXT("/Game/Yarin/ABP_Pangea_MiningLocomotion.ABP_Pangea_MiningLocomotion_C") ||
				Anim->GetCurrentMoveset()->GetClass()->GetPathName() != TEXT("/Game/Yarin/ABP_Pangea_MiningUnarmedMoveset.ABP_Pangea_MiningUnarmedMoveset_C"))
				return Fail(FString::Printf(TEXT("%s reverted to locomotion %s / moveset %s"), *It->GetName(), *Anim->GetClass()->GetPathName(), *Anim->GetCurrentMoveset()->GetClass()->GetPathName()));
			if (Agent->GetCurrentState() == EMiningPresentationState::Traveling && It->GetVelocity().Size2D() > 30 && Anim->GetSpeed() > 10)
			{
				const float GroundSpeed = It->GetVelocity().Size2D();
				if (!bCruiseChecked[Role] && GroundSpeed > It->GetCharacterMovement()->MaxWalkSpeed * .95f)
				{
					UBlendSpace* Blend = MiningObject<UBlendSpace>(Anim->GetCurrentMoveset(), TEXT("MovementBlendspace"));
					if (!Blend || Blend->GetPathName() != TEXT("/Game/Yarin/BS_Pangea_MiningLocomotion.BS_Pangea_MiningLocomotion")) return Fail(TEXT("Active moveset is not using the Pangea walking blendspace"));
					if (!FMath::IsNearlyEqual(Mesh->GlobalAnimRateScale, 1.f) || !FMath::IsNearlyEqual(It->CustomTimeDilation, 1.f)) return Fail(TEXT("Travel has an unintended global animation/time multiplier"));
					TArray<FBlendSampleData> Weights;
					int32 Triangle = INDEX_NONE;
					if (!Blend->GetSamplesFromBlendInput(FVector(0, Anim->GetNormalizedSpeed(), 0), Weights, Triangle, false)) return Fail(TEXT("Live cruising speed has no blendspace coverage"));
					float IdleWeight = 0.f;
					for (const FBlendSampleData& Weight : Weights)
						if (Blend->GetBlendSamples()[Weight.SampleDataIndex].SampleValue.Y == 0.f) IdleWeight += Weight.TotalWeight;
					if (IdleWeight > .01f) return Fail(TEXT("Live cruising locomotion is diluted by idle again"));
					Test.AddInfo(FString::Printf(TEXT("Role %d cruise %.1f cm/s, normalized %.3f, mesh rate %.2f, Pangea moveset, idle weight %.3f"), Role, GroundSpeed, Anim->GetNormalizedSpeed(), Mesh->GlobalAnimRateScale, IdleWeight));
					bCruiseChecked[Role] = true;
				}
				const TArray<FTransform> Pose = Mesh->GetBoneSpaceTransforms();
				const TArray<FTransform>& Ref = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose();
				int32 NonRef = 0, Changed = 0;
				for (int32 I = 1; I < Pose.Num() && I < Ref.Num(); ++I)
				{
					NonRef += !Pose[I].GetRotation().Equals(Ref[I].GetRotation(), 0.015f);
					if (PreviousPose[Role].IsValidIndex(I)) Changed += !Pose[I].GetRotation().Equals(PreviousPose[Role][I].GetRotation(), 0.005f);
				}
				bAnimated[Role] |= NonRef >= 6 && Changed >= 3;
				PreviousPose[Role] = Pose;
			}
			const bool bStation = Agent->GetCurrentState() == EMiningPresentationState::Working || Agent->GetCurrentState() == EMiningPresentationState::Guarding || Agent->GetCurrentState() == EMiningPresentationState::Loading || Agent->GetCurrentState() == EMiningPresentationState::Unloading;
			bStationAnimation[Role] |= bStation && Anim->GetCurrentActiveMontage() != nullptr;
			if (Role == static_cast<int32>(EMiningPresentationRole::Worker))
			{
				UAnimSequenceBase* Active = Agent->GetActivePresentationAnimation();
				bWorkerRetrieval |= Agent->GetCurrentState() == EMiningPresentationState::Retrieving && Active && Active->GetName() == TEXT("AS_Pangea_MinerRetrieve") && Anim->GetCurrentActiveMontage();
				bWorkerDeposit |= Agent->GetCurrentState() == EMiningPresentationState::Depositing && Active && Active->GetName() == TEXT("AS_Pangea_MinerDeposit") && Anim->GetCurrentActiveMontage();
			}
		}
		if (!bArranged) { bArranged = ArrangedCount >= 4; return false; }
		if (!bAnimated[0] || !bAnimated[1] || !bAnimated[2] || !bCruiseChecked[0] || !bCruiseChecked[1] || !bCruiseChecked[2] || !bStationAnimation[0] || !bStationAnimation[1] || !bStationAnimation[2] || !bWorkerRetrieval || !bWorkerDeposit) return false;
		Test.AddInfo(TEXT("All three roles evaluated changing non-reference locomotion poses during path-followed travel and retained station montages"));
		Cleanup();
		return true;
	}
};
}

namespace
{
/** Actual ACF possession/routines/nav, escrow arrival, escort loss and active ALS restore. */
struct FMiningConvoyPIEProbe : FMiningPIEProbe
{
	int32 Stage = 0;
	bool bProtected = false;
	TWeakObjectPtr<AActor> Destination;
	TWeakObjectPtr<UACFStorageComponent> Storage;
	TWeakObjectPtr<AACFCharacter> PreviousCourier;
	TWeakObjectPtr<AACFCharacter> PreviousDraft;
	FVector DraftStart = FVector::ZeroVector;
	bool bDraftMoved = false;
	bool bCheckDraftDeath = false;
	bool bCourierSpeedChecked = false, bDraftSpeedChecked = false, bDraftAnimated = false;
	TArray<FTransform> PreviousDraftPose;
	FGuid Manifest;
	UClass* ItemClass = nullptr;
	explicit FMiningConvoyPIEProbe(FAutomationTestBase& InTest) : FMiningPIEProbe(InTest) { bTransportFixture = true; }
	bool Tick()
	{
		UWorld* World = MiningPIEWorld();
		AACFCharacter* Observer = World && World->GetFirstPlayerController() ? Cast<AACFCharacter>(World->GetFirstPlayerController()->GetPawn()) : nullptr;
		if (Observer)
		{
			Observer->SetCanBeDamaged(false);
			Observer->SetIsImmortal(true);
			if (!bProtected)
			{
				if (Observer->GetActionsComponent()) Observer->GetActionsComponent()->CancelAllAbilities();
				Observer->StopAnimMontage();
				Observer->GetCharacterMovement()->StopMovementImmediately();
				Observer->GetCharacterMovement()->DisableMovement();
				bProtected = true;
			}
		}
		if (Phase < 2) return FMiningPIEProbe::Tick();
		if (!World || !Site.IsValid() || !Observer || World->IsPaused()) return Fail(TEXT("Convoy fixture lost its live unpaused observer/site"));
		auto* Broker = Site->ConvoyComponent.Get();
		auto* Ledger = Site->TransportComponent.Get();
		if (FPlatformTime::Seconds() > Deadline)
		{
			if (Broker && Ledger)
			{
				const auto Trip = Ledger->GetShipment();
				Test.AddInfo(FString::Printf(TEXT("Manifest phase=%d cargo=%d delivered=%d ground=%s origin=%s destination=%s storage=%d"),
					int32(Trip.Phase), Trip.Cargo.Quantity, Trip.DeliveredUnits, *Trip.Position.ToString(), *Trip.Origin.ToString(), *Trip.Destination.ToString(), Storage.IsValid() ? Storage->GetTotalCountOfItemsByClass(ItemClass) : -1));
				for (AACFCharacter* Member : { Broker->GetCourier(), Broker->GetDraftAnimal() })
				{
					if (!IsValid(Member)) continue;
					const auto* AI = Cast<AACFAIController>(Member->GetController());
					const auto* Duty = AI ? AI->FindComponentByClass<UPangeaMiningTransportJobComponent>() : nullptr;
					const auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
					const auto* NavData = Nav ? Nav->GetNavDataForProps(Member->GetNavAgentPropertiesRef()) : nullptr;
					Test.AddInfo(FString::Printf(TEXT("Member %s alive=%d at=%s velocity=%s maxSpeed=%.1f state=%s command=%d duty=%s nav=%s radius=%.1f height=%.1f"),
						*Member->GetName(), Member->IsAlive(), *Member->GetActorLocation().ToString(), *Member->GetVelocity().ToString(), Member->GetCharacterMovement()->MaxWalkSpeed,
						AI ? *AI->GetAIState().ToString() : TEXT("none"), AI && AI->IsExecutingCommand(), Duty ? *Duty->GetBlockedReason() : TEXT("none"),
						*GetNameSafe(NavData), Member->GetNavAgentPropertiesRef().AgentRadius, Member->GetNavAgentPropertiesRef().AgentHeight));
				}
			}
			return Fail(FString::Printf(TEXT("Convoy stage %d timeout: %s"), Stage, Broker ? *Broker->GetBlockedReason() : TEXT("no broker")));
		}
		if (!Broker || !Ledger) return Fail(TEXT("Convoy/ledger reflected components missing; normal rebuild required"));
		if (Stage == 0)
		{
			auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			auto* Linked = Cast<AMiningSettlementStockpileActor>(Site->SettlementResourceActor);
			if (!Linked) return Fail(TEXT("Placed prototype must link its own settlement stockpile"));
			const FVector Goal = Linked->GetCourierUnloadLocation();
			UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Site->GetActorLocation(), Goal);
			if (!Nav || !Path || !Path->IsValid() || Path->IsPartial()) return false; // Wait for invoker tiles, bounded by Deadline.
			AActor* Target = World->SpawnActor<AActor>();
			Target->SetFlags(RF_Transient);
			auto* Root = NewObject<USceneComponent>(Target);
			Target->AddInstanceComponent(Root);
			Target->SetRootComponent(Root);
			Root->RegisterComponent();
			Target->SetActorLocation(Goal); // Isolated inventory, exact placed endpoint; never grant items to the real stockpile.
			auto* Store = NewObject<UPangeaMiningTransportStorageComponent>(Target);
			Target->AddInstanceComponent(Store);
			Store->SetMaxInventorySlots(20);
			Store->SetMaxInventoryWeight(10000.f);
			Store->RegisterComponent();
			Destination = Target;
			Storage = Store;
			Site->SettlementResourceActor = Target;
			FMiningSiteLevelDefinition Level;
			if (!Site->MiningSiteComponent->GetCurrentLevelDefinition(Level)) return Fail(TEXT("Missing convoy fixture tier"));
			FMiningItemQuantity Seed = Level.PrimaryOutput;
			Seed.Quantity = 5;
			ItemClass = Seed.ItemClass;
			if (!ItemClass || Site->MiningSiteComponent->AddStoredItem(Seed) != 5 || !Ledger->TryDispatch()) return Fail(TEXT("Cannot dispatch single escrow fixture"));
			Manifest = Ledger->GetShipment().Id;
			Deadline = FPlatformTime::Seconds() + 45.;
			Stage = 1;
			return false;
		}
		if (Ledger->GetShipment().Id != Manifest) return Fail(TEXT("Visibility/restore changed manifest identity"));
		if (Stage == 1)
		{
			if (!Broker->GetCourier() || Broker->GetEscorts().Num() != 2 || !Broker->GetCart() || !Broker->GetDraftAnimal()) return false;
			PreviousDraft = Broker->GetDraftAnimal();
			if (Broker->GetCart()->GetAttachParentActor() != PreviousDraft.Get()) return Fail(TEXT("Cart is not towed by the independent draft animal"));
			if (PreviousDraft->FindComponentByClass<UMiningPresentationAgentComponent>()) return Fail(TEXT("Humanoid presentation agent was added to Korro"));
			const ANavigationData* AnimalNav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)->GetNavDataForProps(PreviousDraft->GetNavAgentPropertiesRef());
			if (!AnimalNav || AnimalNav->GetConfig().AgentRadius < PreviousDraft->GetSimpleCollisionRadius() || AnimalNav->GetConfig().AgentHeight < PreviousDraft->GetSimpleCollisionHalfHeight() * 2.f)
				return Fail(TEXT("Korro selected undersized navigation data"));
			PreviousCourier = Broker->GetCourier();
			auto* AI = Cast<AACFAIController>(PreviousCourier->GetController());
			if (!AI || !AI->FindComponentByClass<UPangeaMiningTransportJobComponent>()) return Fail(TEXT("Courier not owned by an ACF convoy routine job"));
			AI->SetCurrentAIState(FGameplayTag::RequestGameplayTag(TEXT("AIState.Wait")));
			Broker->GetEscorts()[0]->KillCharacter();
			if (Ledger->GetShipment().LostEscortSlots.Num() != 1) return Fail(TEXT("Real ACF escort death not recorded"));
			const FALSActorData Record = UALSFunctionLibrary::SerializeActor(Site.Get());
			UALSFunctionLibrary::FullDeserializeActor(Site.Get(), Record, false);
			if ((PreviousCourier.IsValid() && !PreviousCourier->IsActorBeingDestroyed()) || Broker->GetCourier() ||
				(PreviousDraft.IsValid() && !PreviousDraft->IsActorBeingDestroyed()) || Broker->GetDraftAnimal()) return Fail(TEXT("ALS restore retained old convoy actors"));
			if (Ledger->GetShipment().Cargo.Quantity != 5 || Storage->GetTotalCountOfItemsByClass(ItemClass) != 0)
				return Fail(FString::Printf(TEXT("Restore lost/duplicated escrow or delivered before arrival: cargo=%d destination=%d phase=%d restoring=%d"), Ledger->GetShipment().Cargo.Quantity, Storage->GetTotalCountOfItemsByClass(ItemClass), int32(Ledger->GetShipment().Phase), Ledger->IsAwaitingRestore()));
			Stage = 2;
			return false;
		}
		if (Stage == 2)
		{
			if (!Broker->GetCourier() || Broker->GetCourier() == PreviousCourier.Get()) return false;
			if (Broker->GetEscorts().Num() != 1 || !Broker->GetCart() || !Broker->GetDraftAnimal() || Broker->GetDraftAnimal() == PreviousDraft.Get()) return Fail(TEXT("Rematerialization resurrected escort slot or lost cart/draft animal"));
			DraftStart = Broker->GetDraftAnimal()->GetActorLocation();
			Stage = 3;
		}
		if (Broker->GetDraftAnimal() && FVector::DistSquared(DraftStart, Broker->GetDraftAnimal()->GetActorLocation()) > FMath::Square(500.)) bDraftMoved = true;
		if (!bCheckDraftDeath && Stage == 3)
		{
			if (auto* Courier = Broker->GetCourier(); Courier && Courier->GetVelocity().Size2D() > 100.f)
				bCourierSpeedChecked |= FMath::IsNearlyEqual(Courier->GetCharacterMovement()->MaxWalkSpeed, Ledger->GetShipment().Speed, 1.f);
			if (auto* Draft = Broker->GetDraftAnimal(); Draft && Draft->GetVelocity().Size2D() > 100.f)
			{
				bDraftSpeedChecked |= FMath::IsNearlyEqual(Draft->GetCharacterMovement()->MaxWalkSpeed, Ledger->GetShipment().Speed * 1.1f, 1.f);
				auto* Mesh = Draft->GetMesh();
				if (!Mesh || !Mesh->GetAnimInstance() || Draft->FindComponentByClass<UMiningPresentationAgentComponent>()) return Fail(TEXT("Draft locomotion lost its native AnimBP or acquired human presentation"));
				const TArray<FTransform>& Pose = Mesh->GetBoneSpaceTransforms();
				if (PreviousDraftPose.Num() == Pose.Num())
					for (int32 Bone = 1; Bone < Pose.Num(); ++Bone)
						if (!Pose[Bone].GetRotation().Equals(PreviousDraftPose[Bone].GetRotation(), .001f)) { bDraftAnimated = true; break; }
				PreviousDraftPose = Pose;
			}
		}
		if (bCheckDraftDeath)
		{
			if (Stage == 3)
			{
				if (!Broker->GetDraftAnimal()) return Fail(TEXT("Draft animal disappeared before casualty test"));
				Broker->GetDraftAnimal()->KillCharacter();
				Stage = 4;
				return false;
			}
			if (Ledger->HasActiveShipment() || Broker->GetCourier()) return false;
			if (!Ledger->GetShipment().bLost || Ledger->GetShipment().Cargo.Quantity != 0 || Storage->GetTotalCountOfItemsByClass(ItemClass) != 0)
				return Fail(TEXT("Draft animal death failed exactly-once cargo loss"));
			Test.AddInfo(TEXT("Actual ACF draft animal death loses remaining escrow without a destination grant or rematerialized animal"));
			Cleanup();
			if (Destination.IsValid()) Destination->Destroy();
			return true;
		}
		if (Storage->GetTotalCountOfItemsByClass(ItemClass) == 0) return false;
		if (Storage->GetTotalCountOfItemsByClass(ItemClass) != 5 || Ledger->GetShipment().Cargo.Quantity != 0) return Fail(TEXT("Arrival failed cargo conservation"));
		if (Ledger->HasActiveShipment() || Broker->GetCourier()) return false;
		if (!bDraftMoved || Broker->GetDraftAnimal()) return Fail(TEXT("Draft animal did not navigate independently or failed retirement"));
		if (!bCourierSpeedChecked || !bDraftSpeedChecked || !bDraftAnimated)
			return Fail(FString::Printf(TEXT("Draft travel acceptance missing: handlerSpeed=%d animalSpeed=%d animatedMovingPose=%d"), bCourierSpeedChecked, bDraftSpeedChecked, bDraftAnimated));
		Test.AddInfo(TEXT("Physical ACF courier, cart and escorts materialized; active ALS restore retired actors without delivery, preserved a dead escort slot, resumed navigation/unload and returned with exactly one destination grant"));
		Cleanup();
		if (Destination.IsValid()) Destination->Destroy();
		return true;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningConvoyPIEAcceptanceTest, "Pangea.ACF.Mining.Transport.PhysicalConvoyAndRestoreInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningConvoyPIEAcceptanceTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FMiningConvoyPIEProbe> Probe = MakeShared<FMiningConvoyPIEProbe>(*this);
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Probe]() { return Probe->Tick(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDraftCasualtyPIETest, "Pangea.ACF.Mining.Transport.DraftAnimalDeathInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDraftCasualtyPIETest::RunTest(const FString& Parameters)
{
	const TSharedRef<FMiningConvoyPIEProbe> Probe = MakeShared<FMiningConvoyPIEProbe>(*this);
	Probe->bCheckDraftDeath = true;
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Probe]() { return Probe->Tick(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAnimationPIEAcceptanceTest, "Pangea.ACF.Mining.RoleLocomotionAndStationAnimationInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningAnimationPIEAcceptanceTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FMiningAnimationPIEProbe> Probe = MakeShared<FMiningAnimationPIEProbe>(*this);
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Probe]() { return Probe->Tick(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

namespace
{
/** Exercise the real ALS actor/component callbacks during an occupied duty.
 * In-memory actor records only: never touch a user's save slot or saved map. */
struct FMiningActiveRestoreProbe : FMiningPIEProbe
{
	bool bObserverPrepared = false;
	explicit FMiningActiveRestoreProbe(FAutomationTestBase& InTest) : FMiningPIEProbe(InTest) {}
	bool Tick()
	{
		UWorld* World = MiningPIEWorld();
		AACFCharacter* Observer = World && World->GetFirstPlayerController() ? Cast<AACFCharacter>(World->GetFirstPlayerController()->GetPawn()) : nullptr;
		if (Observer)
		{
			Observer->SetCanBeDamaged(false);
			Observer->SetIsImmortal(true);
			if (!bObserverPrepared)
			{
				if (Observer->GetActionsComponent()) Observer->GetActionsComponent()->CancelAllAbilities();
				Observer->StopAnimMontage();
				Observer->GetCharacterMovement()->StopMovementImmediately();
				Observer->GetCharacterMovement()->DisableMovement();
				bObserverPrepared = true;
			}
		}
		if (Phase < 2) return FMiningPIEProbe::Tick();
		if (!World || !Site.IsValid() || !Observer || !Observer->IsAlive() || World->IsPaused()) return Fail(TEXT("Active-restore fixture requires a live observer and unpaused world"));
		if (FPlatformTime::Seconds() > Deadline) return Fail(TEXT("Active-duty restore did not resume a fresh occupied worker duty"));
		if (Phase == 2)
		{
			if (!Job.IsValid() || Job->GetJobStatus() != EMiningRoutineJobStatus::Interacting) return false;
			const FName SiteTag(*FString::Printf(TEXT("Mining.Site.%s"), *Site->GetName()));
			TArray<TWeakObjectPtr<AACFCharacter>> Retired;
			TArray<TWeakObjectPtr<UMiningRoutineJobComponent>> RetiredJobs;
			for (TActorIterator<AACFCharacter> It(World); It; ++It)
			{
				if (!It->ActorHasTag(SiteTag)) continue;
				Retired.Add(*It);
				AACFAIController* AI = Cast<AACFAIController>(It->GetController());
				RetiredJobs.Add(AI ? AI->FindComponentByClass<UMiningRoutineJobComponent>() : nullptr);
			}
			AActor* OldChest = MiningObject<AActor>(Site.Get(), TEXT("SpawnedSiteChest"));
			FMiningSiteLevelDefinition Level;
			if (!Site->MiningSiteComponent->GetCurrentLevelDefinition(Level)) return Fail(TEXT("Active restore level missing"));
			FMiningItemQuantity Seed = Level.PrimaryOutput;
			Seed.Quantity = 7;
			if (Site->MiningSiteComponent->AddStoredItem(Seed) != 7) return Fail(TEXT("Cannot seed non-empty restore inventory"));
			const FALSActorData Record = UALSFunctionLibrary::SerializeActor(Site.Get());
			const int32 SavedUnits = Site->MiningSiteComponent->GetStoredUnits();
			const double SavedFraction = Site->MiningSiteComponent->GetProductionRemainder();
			const double SavedCooldown = Site->MiningSiteComponent->GetShipmentCooldownRemaining();
			Seed.Quantity = 3;
			if (Site->MiningSiteComponent->AddStoredItem(Seed) != 3) return Fail(TEXT("Cannot arrange post-save inventory change"));
			for (int32 Load = 0; Load < 2; ++Load)
			{
				UALSFunctionLibrary::FullDeserializeActor(Site.Get(), Record, false);
				if (Site->MiningSiteComponent->GetStoredUnits() != SavedUnits ||
					!FMath::IsNearlyEqual(Site->MiningSiteComponent->GetProductionRemainder(), SavedFraction, 1.e-8) ||
					!FMath::IsNearlyEqual(Site->MiningSiteComponent->GetShipmentCooldownRemaining(), SavedCooldown, 1.e-4))
					return Fail(TEXT("Repeated active-duty restore changed saved inventory/fraction/cooldown"));
			}
			for (const auto& Pawn : Retired) if (Pawn.IsValid() && !Pawn->IsActorBeingDestroyed()) return Fail(TEXT("Restore retained an old presentation pawn"));
			for (const auto& Duty : RetiredJobs) if (Duty.IsValid() && Duty->GetJobStatus() != EMiningRoutineJobStatus::Unassigned) return Fail(TEXT("Restore retained an assigned old job"));
			if (IsValid(OldChest) && !OldChest->IsActorBeingDestroyed()) return Fail(TEXT("Restore retained the old chest"));
			Worker.Reset(); Controller.Reset(); Job.Reset();
			Next(3, 25.0);
			return false;
		}
		Worker = Cast<AACFCharacter>(MiningWorker(Site.Get(), 0));
		Controller = Worker.IsValid() ? Cast<AACFAIController>(Worker->GetController()) : nullptr;
		Job = Controller.IsValid() ? Controller->FindComponentByClass<UMiningRoutineJobComponent>() : nullptr;
		if (!Job.IsValid() || Job->GetJobStatus() != EMiningRoutineJobStatus::Interacting) return false;
		int32 Counts[3] = {};
		const FName SiteTag(*FString::Printf(TEXT("Mining.Site.%s"), *Site->GetName()));
		for (TActorIterator<AACFCharacter> It(World); It; ++It)
		{
			if (!It->ActorHasTag(SiteTag)) continue;
			const UMiningPresentationAgentComponent* Agent = It->FindComponentByClass<UMiningPresentationAgentComponent>();
			if (Agent) ++Counts[static_cast<int32>(Agent->GetCurrentRole())];
		}
		if (Counts[0] != 2 || Counts[1] != 1 || Counts[2] != 1) return Fail(TEXT("Restore duplicated or lost a role actor"));
		Test.AddInfo(TEXT("ALS active-duty actor round trip and repeated restore preserve economics, retire old jobs/chest, recreate exactly one workforce and resume occupied work"));
		Cleanup();
		return true;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningActiveDutyRestoreTest, "Pangea.ACF.Mining.ActiveDutyALSRoundTripInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningActiveDutyRestoreTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FMiningActiveRestoreProbe> Probe = MakeShared<FMiningActiveRestoreProbe>(*this);
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Probe]() { return Probe->Tick(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningRoutinePIEAcceptanceTest, "Pangea.ACF.Mining.NavigationAndInterruptionsInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningRoutinePIEAcceptanceTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FMiningPIEProbe> Probe = MakeShared<FMiningPIEProbe>(*this);
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Probe]() { return Probe->Tick(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningLifecyclePIEAcceptanceTest, "Pangea.ACF.Mining.LifecycleAndContentionInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningLifecyclePIEAcceptanceTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FMiningLifecyclePIEProbe> Probe = MakeShared<FMiningLifecyclePIEProbe>(*this);
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Probe]() { return Probe->Tick(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
