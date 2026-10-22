#include "Components/PangeaMiningTransportComponent.h"

#include "Actors/ACFCharacter.h"
#include "Actors/MiningSettlementStockpileActor.h"
#include "Actors/MiningSiteActor.h"
#include "Components/ACFStorageComponent.h"
#include "Components/MiningSiteComponent.h"
#include "Components/PangeaMiningConvoyComponent.h"
#include "Components/PangeaMiningTransportStorageComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "DataAssets/MiningSitePresentationConfig.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Items/ACFItem.h"
#include "Net/UnrealNetwork.h"

UPangeaMiningTransportComponent::UPangeaMiningTransportComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = .25f;
	SetIsReplicatedByDefault(true);
}

bool UPangeaMiningTransportComponent::HasAuthority() const { return GetOwner() && GetOwner()->HasAuthority(); }
AACFCharacter* UPangeaMiningTransportComponent::GetPhysicalCourier() const { return PhysicalCourier.Get(); }
UMiningSiteComponent* UPangeaMiningTransportComponent::GetSite() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UMiningSiteComponent>() : nullptr;
}
bool UPangeaMiningTransportComponent::IsHybridEnabled() const
{
	const UMiningSiteComponent* Site = GetSite();
	return Site && Site->SiteDefinition && Site->SiteDefinition->bUseHybridTransport;
}
void UPangeaMiningTransportComponent::Publish() { if (HasAuthority()) GetOwner()->ForceNetUpdate(); }

void UPangeaMiningTransportComponent::PrepareForOwnerRestore()
{
	if (GetOwner())
		if (auto* Convoy = GetOwner()->FindComponentByClass<UPangeaMiningConvoyComponent>()) Convoy->ClearPhysicalConvoy();
	if (PhysicalCourier.IsValid()) PhysicalCourier->OnDeath.RemoveDynamic(this, &ThisClass::HandleCourierDeath);
	Shipment = FPangeaMiningShipment();
	RiskSample = 0.f;
	TransportVersion = 0;
	bAwaitingRestore = true;
	bCourierDeathPending = false;
	PhysicalCourier.Reset();
}

void UPangeaMiningTransportComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsSaveGame() && Ar.IsSaving() && PhysicalCourier.IsValid())
		Shipment.Position = PhysicalCourier->GetActorLocation() - FVector(0, 0, PhysicalCourier->GetSimpleCollisionHalfHeight());
	if (Ar.IsSaveGame() && Ar.IsLoading())
	{
		PrepareForOwnerRestore();
	}
	const bool bNoDelta = !Ar.DoDelta();
	if (Ar.IsSaveGame() && Ar.IsSaving()) Ar.ArNoDelta = true;
	Super::Serialize(Ar);
	Ar.ArNoDelta = bNoDelta;
}

void UPangeaMiningTransportComponent::OnComponentLoaded()
{
	if (!HasAuthority() || !bAwaitingRestore) return;
	if (TransportVersion < 0 || TransportVersion > 3) return; // Fail closed; preserve unknown cargo.
	if (TransportVersion == 0) Shipment = FPangeaMiningShipment();
	if (TransportVersion == 1 && HasActiveShipment())
	{
		FMiningSiteLevelDefinition Level;
		if (const auto* Site = GetSite(); Site && Site->GetCurrentLevelDefinition(Level))
		{
			Shipment.CourierClass = Level.CourierClass;
			Shipment.EscortClass = Level.GuardClass;
			Shipment.EscortCount = FMath::Clamp(Level.TransportEscortCount, 0, 8);
			Shipment.ChariotClass = Level.TransportChariotClass;
		}
	}
	if (Shipment.Position.ContainsNaN() || Shipment.Origin.ContainsNaN() || Shipment.Destination.ContainsNaN() ||
		!FMath::IsFinite(Shipment.Speed) || Shipment.Speed <= 0.f || Shipment.Cargo.Quantity < 0 ||
		Shipment.DeliveredUnits < 0 || Shipment.DeliveredUnits > MAX_int32 - Shipment.Cargo.Quantity ||
		(Shipment.Phase == EPangeaMiningShipmentPhase::Idle && Shipment.Cargo.Quantity > 0) ||
		(Shipment.Phase != EPangeaMiningShipmentPhase::Idle && !Shipment.Id.IsValid()) ||
		!FMath::IsFinite(RiskSample) || RiskSample < 0.f || RiskSample >= 1.f ||
		!FMath::IsFinite(Shipment.LossChance) || Shipment.LossChance < 0.f || Shipment.LossChance > 1.f ||
		!FMath::IsFinite(Shipment.VirtualTravelSeconds) || Shipment.VirtualTravelSeconds < 0.0 ||
		!FMath::IsFinite(Shipment.PlannedOutboundSeconds) || Shipment.PlannedOutboundSeconds <= 0.0 ||
		static_cast<uint8>(Shipment.Phase) > static_cast<uint8>(EPangeaMiningShipmentPhase::Returning)) return;
	if (Shipment.EscortCount < 0 || Shipment.EscortCount > 8) return;
	TSet<int32> Slots;
	for (int32 Slot : Shipment.LostEscortSlots)
	{
		if (Slot < 0 || Slot >= Shipment.EscortCount || Slots.Contains(Slot)) return;
		Slots.Add(Slot);
	}
	// v1/v2 manifests keep their existing on-foot presentation. Never retrofit a
	// draft animal or a faster speed into a journey already in progress.
	TransportVersion = 3;
	bAwaitingRestore = false;
	Publish(); // No wall-clock catch-up, movement, delivery or reroll during load.
}

bool UPangeaMiningTransportComponent::TryDispatch()
{
	if (!HasAuthority() || !IsHybridEnabled() || bAwaitingRestore || bOperationInProgress || HasActiveShipment()) return false;
	AMiningSiteActor* Actor = Cast<AMiningSiteActor>(GetOwner());
	UMiningSiteComponent* Site = GetSite();
	AActor* DestinationActor = Actor ? Actor->SettlementResourceActor.Get() : nullptr;
	auto* Storage = IsValid(DestinationActor) ? DestinationActor->FindComponentByClass<UPangeaMiningTransportStorageComponent>() : nullptr;
	FMiningSiteLevelDefinition Level;
	if (!Site || !Storage || DestinationActor == GetOwner() || !Site->GetCurrentLevelDefinition(Level) ||
		!FMath::IsFinite(Level.TransportTravelSpeed) || Level.TransportTravelSpeed <= 0.f ||
		Level.TransportEscortCount < 0 || Level.TransportEscortCount > 8 ||
		(!Level.TransportDraftAnimalClass.IsNull() && Level.TransportChariotClass.IsNull()) ||
		!FMath::IsFinite(Level.ShipmentLossChance) || Level.ShipmentLossChance < 0.f || Level.ShipmentLossChance > 1.f ||
		GetOwner()->GetActorLocation().ContainsNaN() || DestinationActor->GetActorLocation().ContainsNaN()) return false;
	TGuardValue<bool> Guard(bOperationInProgress, true);
	FMiningItemQuantity Cargo;
	if (!Site->ReserveTransportCargo(Cargo)) return false;
	Shipment = FPangeaMiningShipment();
	Shipment.Id = FGuid::NewGuid();
	Shipment.Phase = EPangeaMiningShipmentPhase::Outbound;
	Shipment.Cargo.ItemClass = Cargo.ItemClass;
	Shipment.Cargo.ItemTag = Cargo.ItemTag;
	Shipment.Cargo.Quantity = Cargo.Quantity;
	Shipment.DestinationActor = DestinationActor;
	Shipment.Origin = GetOwner()->GetActorLocation();
	Shipment.Destination = DestinationActor->GetActorLocation();
	if (const AMiningSettlementStockpileActor* Stockpile = Cast<AMiningSettlementStockpileActor>(DestinationActor))
		Shipment.Destination = Stockpile->GetCourierUnloadLocation();
	Shipment.Position = Shipment.Origin;
	Shipment.Speed = Level.TransportTravelSpeed;
	Shipment.LossChance = Level.ShipmentLossChance;
	Shipment.CourierClass = Level.CourierClass;
	Shipment.EscortClass = Level.GuardClass;
	Shipment.ChariotClass = Level.TransportChariotClass;
	Shipment.DraftAnimalClass = Level.TransportDraftAnimalClass;
	Shipment.EscortCount = FMath::Clamp(Level.TransportEscortCount, 0, 8);
	if (const auto* Config = Cast<UMiningSitePresentationConfig>(Site->SiteDefinition->PresentationConfig.LoadSynchronous()))
	{
		if (!Config->CourierRole.ActorClassOverride.IsNull()) Shipment.CourierClass = Config->CourierRole.ActorClassOverride;
		if (!Config->GuardRole.ActorClassOverride.IsNull()) Shipment.EscortClass = Config->GuardRole.ActorClassOverride;
	}
	Shipment.PlannedOutboundSeconds = FMath::Max(.001, FVector::Distance(Shipment.Origin, Shipment.Destination) / Shipment.Speed);
	RiskSample = FMath::FRand(); // One saved server-only sample per manifest; never rolled on spawning/loading.
	Publish();
	return true;
}

bool UPangeaMiningTransportComponent::RecordEscortLoss(FGuid ShipmentId, int32 Slot)
{
	if (!HasAuthority() || bAwaitingRestore || !HasActiveShipment() || Shipment.Id != ShipmentId ||
		Slot < 0 || Slot >= Shipment.EscortCount || Shipment.LostEscortSlots.Contains(Slot)) return false;
	Shipment.LostEscortSlots.Add(Slot);
	Publish();
	return true;
}

bool UPangeaMiningTransportComponent::ShouldMaterializeCourier() const
{
	if (!HasActiveShipment() || !GetWorld()) return false;
	const double Radius = FMath::IsFinite(MaterializationRadius) ? FMath::Max(1.0, double(MaterializationRadius)) : 2500.0;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (IsValid(Pawn) && FVector::DistSquared(Pawn->GetActorLocation(), Shipment.Position) <= Radius * Radius) return true;
	}
	return false;
}

bool UPangeaMiningTransportComponent::BindPhysicalCourier(AACFCharacter* Courier, FGuid ShipmentId)
{
	if (!HasAuthority() || bAwaitingRestore || !HasActiveShipment() || Shipment.Id != ShipmentId || !IsValid(Courier) ||
		!Courier->IsAlive() || (PhysicalCourier.IsValid() && PhysicalCourier.Get() != Courier) ||
		!Courier->GetController() || Courier->GetController()->GetOwner() != GetOwner() ||
		FVector::DistSquared(Courier->GetActorLocation(), Shipment.Position) > FMath::Square(500.0)) return false;
	PhysicalCourier = Courier;
	Courier->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleCourierDeath);
	return true;
}

void UPangeaMiningTransportComponent::UnbindPhysicalCourier(AACFCharacter* Courier)
{
	if (!HasAuthority() || !Courier || PhysicalCourier.Get() != Courier) return;
	if (IsValid(Courier))
	{
		Shipment.Position = Courier->GetActorLocation();
		// Keep the virtual ground trace on the nav surface, not a capsule-height
		// staircase after every visibility transition.
		Shipment.Position.Z -= Courier->GetSimpleCollisionHalfHeight();
		Courier->OnDeath.RemoveDynamic(this, &ThisClass::HandleCourierDeath);
	}
	PhysicalCourier.Reset(); // Culling is NOT loss of cargo or an NPC death.
	Publish();
}

void UPangeaMiningTransportComponent::HandleCourierDeath(AACFCharacter* Courier)
{
	if (!HasAuthority() || PhysicalCourier.Get() != Courier || !HasActiveShipment()) return;
	// Inventory delegates may synchronously kill the courier while an unload is
	// committing. Finish counting accepted items before dropping remaining cargo.
	if (bOperationInProgress) { bCourierDeathPending = true; return; }
	bCourierDeathPending = false;
	Shipment.Cargo.Quantity = 0;
	Shipment.bLost = true;
	Shipment.bRiskResolved = true;
	Shipment.Phase = EPangeaMiningShipmentPhase::Idle;
	UnbindPhysicalCourier(Courier);
	Publish();
}

void UPangeaMiningTransportComponent::TryUnload()
{
	if (Shipment.Phase != EPangeaMiningShipmentPhase::AwaitingUnload) return;
	if (!Shipment.bRiskResolved)
	{
		// Hidden risk applies only to virtual exposure. Visible danger uses actual ACF combat.
		const double Exposure = FMath::Clamp(Shipment.VirtualTravelSeconds / Shipment.PlannedOutboundSeconds, 0.0, 1.0);
		const double Chance = 1.0 - FMath::Pow(1.0 - double(Shipment.LossChance), Exposure);
		Shipment.bRiskResolved = true; // Commit before inventory delegates/retries.
		Shipment.bLost = RiskSample < Chance;
		if (Shipment.bLost) Shipment.Cargo.Quantity = 0;
	}
	UClass* ItemClass = Shipment.Cargo.ItemClass.Get();
	auto* Storage = IsValid(Shipment.DestinationActor) ? Shipment.DestinationActor->FindComponentByClass<UPangeaMiningTransportStorageComponent>() : nullptr;
	if (Shipment.Cargo.Quantity > 0 && Storage && ItemClass && ItemClass->IsChildOf(UACFItem::StaticClass()))
	{
		const int32 Accepted = Storage->AcceptShipmentItems(FBaseItem(ItemClass, Shipment.Cargo.Quantity));
		Shipment.Cargo.Quantity -= Accepted;
		Shipment.DeliveredUnits += Accepted;
	}
	// Keep rejected/remaining cargo in escrow. Missing/full destination never deletes it.
	if (Shipment.Cargo.Quantity == 0) Shipment.Phase = EPangeaMiningShipmentPhase::Returning;
	Publish();
}

bool UPangeaMiningTransportComponent::CompletePhysicalLeg(AACFCharacter* Courier, FGuid ShipmentId)
{
	if (!HasAuthority() || bAwaitingRestore || bOperationInProgress || PhysicalCourier.Get() != Courier ||
		!IsValid(Courier) || !Courier->IsAlive() || Shipment.Id != ShipmentId || !HasActiveShipment()) return false;
	const FVector Goal = Shipment.Phase == EPangeaMiningShipmentPhase::Returning ? Shipment.Origin : Shipment.Destination;
	if (FVector::DistSquared(Courier->GetActorLocation(), Goal) > FMath::Square(200.0)) return false;
	if (!Shipment.DraftAnimalClass.IsNull())
	{
		const auto* Convoy = GetOwner()->FindComponentByClass<UPangeaMiningConvoyComponent>();
		if (!Convoy || !Convoy->IsDraftReadyForArrival(Goal)) return false;
	}
	TGuardValue<bool> Guard(bOperationInProgress, true);
	Shipment.Position = Courier->GetActorLocation() - FVector(0, 0, Courier->GetSimpleCollisionHalfHeight());
	if (Shipment.Phase == EPangeaMiningShipmentPhase::Returning) Shipment.Phase = EPangeaMiningShipmentPhase::Idle;
	else { Shipment.Phase = EPangeaMiningShipmentPhase::AwaitingUnload; TryUnload(); }
	Publish();
	return true;
}

void UPangeaMiningTransportComponent::AdvanceVirtual(double Seconds)
{
	if (!FMath::IsFinite(Seconds) || Seconds <= 0.0) return;
	if (Shipment.Phase == EPangeaMiningShipmentPhase::AwaitingUnload) { TryUnload(); return; }
	const bool bOutbound = Shipment.Phase == EPangeaMiningShipmentPhase::Outbound;
	if (!bOutbound && Shipment.Phase != EPangeaMiningShipmentPhase::Returning) return;
	const FVector Goal = bOutbound ? Shipment.Destination : Shipment.Origin;
	const FVector Delta = Goal - Shipment.Position;
	const double Distance = Delta.Size();
	double Step = FMath::Min(Distance, double(Shipment.Speed) * Seconds);
	// Long frames must not leap through an observer's radius or deliver invisibly.
	const FVector Direction = Delta.GetSafeNormal();
	const double Radius = FMath::IsFinite(MaterializationRadius) ? FMath::Max(1., double(MaterializationRadius)) : 2500.;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Observer = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (!IsValid(Observer)) continue;
		const FVector Offset = Shipment.Position - Observer->GetActorLocation();
		const double B = FVector::DotProduct(Offset, Direction);
		const double Discriminant = B * B - (Offset.SizeSquared() - Radius * Radius);
		if (Discriminant < 0.) continue;
		const double Entry = -B - FMath::Sqrt(Discriminant);
		if (Entry >= 0. && Entry < Step) Step = FMath::Min(Step, Entry + .1);
	}
	if (bOutbound) Shipment.VirtualTravelSeconds += Step / Shipment.Speed;
	Shipment.Position += Delta.GetSafeNormal() * Step;
	if (ShouldMaterializeCourier()) { Publish(); return; }
	if (Distance - Step <= 1.0)
	{
		Shipment.Position = Goal;
		Shipment.Phase = bOutbound ? EPangeaMiningShipmentPhase::AwaitingUnload : EPangeaMiningShipmentPhase::Idle;
		if (bOutbound) TryUnload();
	}
	Publish();
}

void UPangeaMiningTransportComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!HasAuthority() || !IsHybridEnabled() || bAwaitingRestore || bOperationInProgress) return;
	if (bCourierDeathPending)
	{
		bCourierDeathPending = false;
		if (PhysicalCourier.IsValid()) HandleCourierDeath(PhysicalCourier.Get());
	}
	if (!HasActiveShipment()) { TryDispatch(); return; }
	TGuardValue<bool> Guard(bOperationInProgress, true);
	if (PhysicalCourier.IsValid())
	{
		Shipment.Position = PhysicalCourier->GetActorLocation() - FVector(0, 0, PhysicalCourier->GetSimpleCollisionHalfHeight());
		Publish();
		return;
	}
	// Fail closed near observers until a physical actor is bound; never deliver invisibly
	// because navigation/materialization failed. The separate convoy broker binds it.
	if (ShouldMaterializeCourier()) return;
	AdvanceVirtual(DeltaTime);
}

void UPangeaMiningTransportComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPangeaMiningTransportComponent, Shipment);
}
