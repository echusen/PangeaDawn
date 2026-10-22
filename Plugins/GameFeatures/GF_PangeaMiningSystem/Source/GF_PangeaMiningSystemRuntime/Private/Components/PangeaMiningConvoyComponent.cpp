#include "Components/PangeaMiningConvoyComponent.h"

#include "ACFAIController.h"
#include "Actors/ACFCharacter.h"
#include "Actors/MiningSiteActor.h"
#include "Components/MiningSitePresentationCoordinatorComponent.h"
#include "Components/PangeaMiningTransportComponent.h"
#include "Components/PangeaMiningTransportJobComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationInvokerComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Runtime/MiningPresentationActorRetirement.h"

UPangeaMiningConvoyComponent::UPangeaMiningConvoyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = .25f;
}
UPangeaMiningTransportComponent* UPangeaMiningConvoyComponent::GetTransport() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UPangeaMiningTransportComponent>() : nullptr;
}

AACFCharacter* UPangeaMiningConvoyComponent::GetCourier() const { return Courier; }
AActor* UPangeaMiningConvoyComponent::GetCart() const { return Cart; }
AACFCharacter* UPangeaMiningConvoyComponent::GetDraftAnimal() const { return DraftAnimal; }

bool UPangeaMiningConvoyComponent::IsDraftReadyForArrival(const FVector& Goal) const
{
	const auto* Transport = GetTransport();
	if (!Transport || ManifestId != Transport->GetShipment().Id) return false;
	if (Transport->GetShipment().DraftAnimalClass.IsNull()) return true;
	const auto* AI = IsValid(DraftAnimal) ? Cast<AACFAIController>(DraftAnimal->GetController()) : nullptr;
	return IsValid(DraftAnimal) && DraftAnimal->IsAlive() && IsValid(Cart) && Cart->GetAttachParentActor() == DraftAnimal &&
		AI && AI->GetOwner() == GetOwner() && !AI->IsExecutingCommand() &&
		AI->GetAIState().MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("AIState.Routine"))) &&
		FVector::DistSquared(DraftAnimal->GetActorLocation(), Goal) <= FMath::Square(450.);
}

TArray<AACFCharacter*> UPangeaMiningConvoyComponent::GetEscorts() const
{
	TArray<AACFCharacter*> Result;
	for (AACFCharacter* Escort : Escorts) if (IsValid(Escort)) Result.Add(Escort);
	return Result;
}

AACFCharacter* UPangeaMiningConvoyComponent::SpawnMember(UClass* Class, const FVector& GroundLocation, int32 EscortSlot)
{
	auto* Site = Cast<AMiningSiteActor>(GetOwner());
	auto* Coordinator = Site ? Cast<UMiningSitePresentationCoordinatorComponent>(Site->PresentationCoordinator) : nullptr;
	const auto* Config = Coordinator ? Coordinator->GetPresentationConfig(Site->MiningSiteComponent) : nullptr;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Point;
	if (!Site || !Coordinator || !Config || !Config->bUseACFRoutineJobs || !Class || !Class->IsChildOf(AACFCharacter::StaticClass()) || !Nav) return nullptr;
	const AACFCharacter* Defaults = CastChecked<AACFCharacter>(Class->GetDefaultObject());
	const UCapsuleComponent* Capsule = Defaults->GetCapsuleComponent();
	if (!Capsule) return nullptr;
	// ACharacter's GetSimpleCollision* getters deliberately return zero on CDOs.
	// Read the authored capsule template instead, including its component scale.
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	FNavAgentProperties Agent = Defaults->GetNavAgentPropertiesRef();
	if (EscortSlot == DraftAnimalSlot)
	{
		// A CDO's movement component has not received its live capsule update yet.
		Agent.AgentRadius = Radius;
		Agent.AgentHeight = HalfHeight * 2.f;
	}
	const ANavigationData* NavData = Nav->GetNavDataForProps(Agent, GroundLocation);
	if (!NavData) return nullptr;
	// Never silently use the 35cm humanoid navmesh for a 72cm Korro capsule.
	if (EscortSlot == DraftAnimalSlot &&
		(NavData->GetConfig().AgentRadius < Radius || NavData->GetConfig().AgentHeight < HalfHeight * 2.f)) return nullptr;
	if (!Nav->ProjectPointToNavigation(GroundLocation, Point, FVector(200, 200, 400), NavData)) return nullptr;
	const FVector Spawn = Point.Location + FVector(0, 0, HalfHeight);
	AACFCharacter* Member = Cast<AACFCharacter>(Coordinator->SpawnConfiguredPresentationActor(*Site, Class, Spawn));
	AACFAIController* AI = Member ? Cast<AACFAIController>(Member->GetController()) : nullptr;
	if (!Member || !AI || AI->GetOwner() != Site) { RetireMember(Member); return nullptr; }
	if (EscortSlot == DraftAnimalSlot)
		if (auto* Following = Cast<UCrowdFollowingComponent>(AI->GetPathFollowingComponent()))
			// DetourCrowd owns a single navmesh. Use normal path following on
			// Korro's larger-agent mesh; leave humanoid crowd simulation untouched.
			Following->SetCrowdSimulationState(ECrowdSimulationState::Disabled);
	Member->SetReplicates(true);
	Member->SetReplicateMovement(true);
	Member->Tags.AddUnique(TEXT("Pangea.Mining.Convoy"));
	// A physical convoy can move beyond the player's invoker. This component is
	// owned by this transient pawn and cannot unregister another actor's invoker.
	auto* Invoker = NewObject<UNavigationInvokerComponent>(Member, NAME_None, RF_Transient);
	Member->AddInstanceComponent(Invoker);
	Invoker->SetGenerationRadii(4000.f, 4500.f);
	Invoker->RegisterComponent();
	if (!Invoker->IsActive()) Invoker->Activate();
	// Deliberately not Mining.Site/Mining.Courier: station population cleanup must
	// not retire a manifest's actors when the camp loses relevance or upgrades.
	auto* Job = NewObject<UPangeaMiningTransportJobComponent>(AI, NAME_None, RF_Transient);
	AI->AddInstanceComponent(Job);
	Job->RegisterComponent();
	if (!Job->Configure(this, Member, EscortSlot)) { RetireMember(Member); return nullptr; }
	return Member;
}

bool UPangeaMiningConvoyComponent::Materialize()
{
	UPangeaMiningTransportComponent* Transport = GetTransport();
	if (!Transport || !Transport->HasActiveShipment() || Transport->IsAwaitingRestore()) return false;
	const FPangeaMiningShipment Trip = Transport->GetShipment();
	ManifestId = Trip.Id;
	Courier = SpawnMember(Trip.CourierClass.LoadSynchronous(), Trip.Position, INDEX_NONE);
	if (!Courier || !Transport->BindPhysicalCourier(Courier, Trip.Id))
	{
		BlockedReason = TEXT("Courier/configuration/navigation could not materialize; cargo retained");
		ClearPhysicalConvoy();
		return false;
	}
	if (!Trip.DraftAnimalClass.IsNull())
	{
		// The handler leads on foot. Korro keeps its own animation instance and
		// ACF navigation; it is never moved by attaching it to the human courier.
		DraftAnimal = SpawnMember(Trip.DraftAnimalClass.LoadSynchronous(), Trip.Position + FVector(-200, 160, 0), DraftAnimalSlot);
		if (!DraftAnimal)
		{
			BlockedReason = TEXT("Configured draft animal could not navigate/materialize; cargo retained");
			ClearPhysicalConvoy();
			return false;
		}
		DraftAnimal->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleDraftAnimalDeath);
	}
	Escorts.SetNum(Trip.EscortCount);
	for (int32 Slot = 0; Slot < Trip.EscortCount; ++Slot)
	{
		if (Trip.LostEscortSlots.Contains(Slot)) continue;
		const FVector Offset(-180 - (Slot / 2) * 100, Slot % 2 ? 150 : -150, 0);
		Escorts[Slot] = SpawnMember(Trip.EscortClass.LoadSynchronous(), Trip.Position + Offset, Slot);
		if (!Escorts[Slot])
		{
			BlockedReason = TEXT("Configured escort could not materialize; whole convoy rolled back");
			ClearPhysicalConvoy();
			return false;
		}
		Escorts[Slot]->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleEscortDeath);
	}
	if (!Trip.ChariotClass.IsNull())
	{
		UClass* CartClass = Trip.ChariotClass.LoadSynchronous();
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// A cart is presentation only. Never create a second character/container here.
		if (CartClass && !CartClass->IsChildOf(APawn::StaticClass()))
			Cart = GetWorld()->SpawnActor<AActor>(CartClass, Courier->GetActorTransform(), Params);
		if (!Cart || !Cart->GetRootComponent())
		{
			BlockedReason = TEXT("Configured cart unavailable; convoy retained in escrow for retry");
			ClearPhysicalConvoy();
			return false;
		}
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Cart);
		for (auto* Primitive : Primitives)
		{
			Primitive->SetMobility(EComponentMobility::Movable);
			Primitive->SetSimulatePhysics(false);
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Cart->SetReplicates(true);
		Cart->SetReplicateMovement(true);
		Cart->AttachToActor(DraftAnimal ? DraftAnimal.Get() : Courier.Get(), FAttachmentTransformRules::KeepRelativeTransform);
		Cart->SetActorRelativeTransform(DraftAnimal ? DraftCartRelativeTransform : CartRelativeTransform);
	}
	BlockedReason.Reset();
	return true;
}

bool UPangeaMiningConvoyComponent::CanDematerialize() const
{
	const auto* Transport = GetTransport();
	const double Radius = FMath::Max(double(Transport ? Transport->MaterializationRadius : 2500.f) + 100.,
		FMath::IsFinite(DematerializationRadius) ? double(DematerializationRadius) : 3000.);
	TArray<AACFCharacter*> Members;
	if (IsValid(Courier)) Members.Add(Courier);
	if (IsValid(DraftAnimal)) Members.Add(DraftAnimal);
	for (AACFCharacter* Escort : Escorts) if (IsValid(Escort)) Members.Add(Escort);
	for (const AACFCharacter* Member : Members)
	{
		const auto* AI = Cast<AACFAIController>(Member->GetController());
		// Do not erase visible fights or external commands by changing fidelity.
		if (Member->IsAlive() && (!AI || AI->IsInBattle() || AI->IsExecutingCommand())) return false;
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (IsValid(Pawn) && FVector::DistSquared(Pawn->GetActorLocation(), Member->GetActorLocation()) < Radius * Radius) return false;
		}
	}
	return true;
}

void UPangeaMiningConvoyComponent::RetireMember(AACFCharacter* Member)
{
	if (!IsValid(Member)) return;
	Member->OnDeath.RemoveDynamic(this, &ThisClass::HandleEscortDeath);
	Member->OnDeath.RemoveDynamic(this, &ThisClass::HandleDraftAnimalDeath);
	AController* Controller = Member->GetController();
	const bool bOwnsController = IsValid(Controller) && Controller->GetOwner() == GetOwner();
	if (bOwnsController)
	{
		if (auto* Job = Controller->FindComponentByClass<UPangeaMiningTransportJobComponent>()) Job->ReleaseAssignment();
		Controller->UnPossess();
	}
	PangeaMiningPrivate::RetirePresentationActor(Member);
	if (bOwnsController) Controller->Destroy();
}

void UPangeaMiningConvoyComponent::ClearPhysicalConvoy()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bClearing) return;
	if (!Courier && !DraftAnimal && Escorts.IsEmpty() && !Cart) { ManifestId.Invalidate(); return; }
	TGuardValue<bool> Guard(bClearing, true);
	if (auto* Transport = GetTransport()) Transport->UnbindPhysicalCourier(Courier);
	if (IsValid(Cart)) Cart->Destroy();
	Cart = nullptr;
	for (AACFCharacter* Escort : Escorts) RetireMember(Escort);
	Escorts.Reset();
	RetireMember(DraftAnimal);
	DraftAnimal = nullptr;
	RetireMember(Courier);
	Courier = nullptr;
	ManifestId.Invalidate();
}

void UPangeaMiningConvoyComponent::HandleEscortDeath(AACFCharacter* Escort)
{
	if (bClearing) return;
	const int32 Slot = Escorts.IndexOfByKey(Escort);
	if (auto* Transport = GetTransport(); Transport && Slot != INDEX_NONE) Transport->RecordEscortLoss(ManifestId, Slot);
	// Retain the corpse until convoy retirement; never respawn this manifest's lost slot.
}

void UPangeaMiningConvoyComponent::HandleDraftAnimalDeath(AACFCharacter* Animal)
{
	if (bClearing || Animal != DraftAnimal || !IsValid(Animal) || Animal->IsAlive()) return;
	if (auto* Transport = GetTransport(); Transport && ManifestId == Transport->GetShipment().Id)
		Transport->HandleCourierDeath(Courier); // Same deferred, exactly-once cargo loss contract.
}

void UPangeaMiningConvoyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority() || bClearing) return;
	auto* Transport = GetTransport();
	if (!Transport || Transport->IsAwaitingRestore()) return;
	if (!Transport->IsHybridEnabled() || !Transport->HasActiveShipment())
	{
		ClearPhysicalConvoy();
		if (IsValid(NavigationInvoker)) { NavigationInvoker->Deactivate(); NavigationInvoker->DestroyComponent(); }
		NavigationInvoker = nullptr;
		return;
	}
	if (Transport->ShouldMaterializeCourier() && !IsValid(NavigationInvoker))
	{
		NavigationInvoker = NewObject<UNavigationInvokerComponent>(GetOwner(), NAME_None, RF_Transient);
		GetOwner()->AddInstanceComponent(NavigationInvoker);
		NavigationInvoker->SetGenerationRadii(4000.f, 4500.f);
		NavigationInvoker->RegisterComponent();
		if (!NavigationInvoker->IsActive()) NavigationInvoker->Activate();
	}
	if (Courier && (!IsValid(Courier) || ManifestId != Transport->GetShipment().Id || Transport->GetPhysicalCourier() != Courier))
		ClearPhysicalConvoy();
	if (IsValid(Courier))
	{
		// The ledger may be deferring a death raised by an inventory delegate until
		// its transaction finishes. Do not turn that dead courier into virtual travel.
		if (!Courier->IsAlive()) return;
		// Unexpected destruction is not a legitimate visibility transition. Do not
		// erase the cart or silently deliver without its configured draft animal.
		if (!Transport->GetShipment().DraftAnimalClass.IsNull() && !IsValid(DraftAnimal))
		{
			BlockedReason = TEXT("Draft animal disappeared; shipment paused for recovery");
			return;
		}
		if (IsValid(DraftAnimal) && !DraftAnimal->IsAlive()) return;
		if (CanDematerialize()) ClearPhysicalConvoy();
		return;
	}
	if (GetWorld()->GetTimeSeconds() < RetryAt || !Transport->ShouldMaterializeCourier()) return;
	if (!Materialize()) RetryAt = GetWorld()->GetTimeSeconds() + 3.;
}

void UPangeaMiningConvoyComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearPhysicalConvoy();
	if (IsValid(NavigationInvoker)) { NavigationInvoker->Deactivate(); NavigationInvoker->DestroyComponent(); }
	NavigationInvoker = nullptr;
	Super::EndPlay(Reason);
}
