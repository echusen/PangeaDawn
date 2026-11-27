#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "Types/MiningTypes.h"

#include "PangeaMiningTransportComponent.generated.h"

class AACFCharacter;
class AActor;
class FLifetimeProperty;
class UMiningSiteComponent;

/** Stage of a hybrid mining shipment's round trip between the site and its settlement destination. */
UENUM(BlueprintType)
enum class EPangeaMiningShipmentPhase : uint8
{
	/** No shipment is active. */
	Idle,
	/** Carrying cargo from the mining site to the destination. */
	Outbound,
	/** At the destination with cargo that the destination storage has not yet fully accepted. */
	AwaitingUnload,
	/** Travelling back to the mining site with no cargo left in escrow. */
	Returning
};

/** The authoritative cargo ledger. Presentation actors never own or duplicate its items. */
USTRUCT(BlueprintType)
struct GF_PANGEAMININGSYSTEMRUNTIME_API FPangeaMiningShipment
{
	GENERATED_BODY()
	/** Unique id of this manifest; physical convoy actors must present it to act on the shipment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	FGuid Id;

	/** Current stage of the round trip. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	EPangeaMiningShipmentPhase Phase = EPangeaMiningShipmentPhase::Idle;

	/** Item and quantity still held in escrow by the shipment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	FMiningStoredItem Cargo;

	/** Actor whose transport storage component receives the cargo. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	TObjectPtr<AActor> DestinationActor;

	/** World location of the mining site when the shipment was dispatched. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	FVector Origin = FVector::ZeroVector;

	/** World location where the cargo is unloaded. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	FVector Destination = FVector::ZeroVector;

	/** Current ground position of the shipment, whether it is virtual or carried by a physical courier. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	FVector Position = FVector::ZeroVector;

	/** Travel speed in cm/s, fixed when the shipment was dispatched. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	float Speed = 320.f;

	/** Chance (0 to 1) of losing the cargo over a fully virtual outbound trip. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	float LossChance = 0.f;

	/** Seconds of the outbound trip that were simulated without a physical courier. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	double VirtualTravelSeconds = 0.0;

	/** Expected duration of the outbound trip in seconds; scales the hidden loss risk by virtual exposure. */
	UPROPERTY(SaveGame)
	double PlannedOutboundSeconds = 1.0;

	/** Whether the hidden loss roll for this shipment has already been applied. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	bool bRiskResolved = false;

	/** Whether the cargo was lost, either to the hidden risk roll or to a death in the physical convoy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	bool bLost = false;

	/** Units accepted so far by the destination storage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	int32 DeliveredUnits = 0;

	/** Convoy configuration is fixed for this manifest, not changed mid-trip by upgrades. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	TSoftClassPtr<AActor> CourierClass;

	/** Class of the escort characters spawned with the physical convoy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	TSoftClassPtr<AActor> EscortClass;

	/** Optional class of the cosmetic cart attached to the convoy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	TSoftClassPtr<AActor> ChariotClass;

	/** Optional class of the draft animal that tows the cart. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	TSoftClassPtr<AActor> DraftAnimalClass;

	/** Number of escort slots of this manifest (0 to 8). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	int32 EscortCount = 0;

	/** Escort slots whose escort died; they are not respawned for this manifest. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Transport")
	TArray<int32> LostEscortSlots;
};

/** Hybrid logistics foundation. Enable only after the ACF physical-actor broker is configured. */
UCLASS(ClassGroup = (Pangea), BlueprintType, meta = (BlueprintSpawnableComponent))
class GF_PANGEAMININGSYSTEMRUNTIME_API UPangeaMiningTransportComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPangeaMiningTransportComponent();
	virtual void Serialize(FArchive& Ar) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** True when the owner's mining site definition enables hybrid transport. */
	UFUNCTION(BlueprintPure, Category = "Mining|Transport")
	bool IsHybridEnabled() const;

	/** True while a shipment is in any phase other than Idle. */
	UFUNCTION(BlueprintPure, Category = "Mining|Transport")
	bool HasActiveShipment() const
	{
		return Shipment.Phase != EPangeaMiningShipmentPhase::Idle;
	}

	/** Copy of the current shipment manifest. */
	UFUNCTION(BlueprintPure, Category = "Mining|Transport")
	FPangeaMiningShipment GetShipment() const
	{
		return Shipment;
	}

	/** True when a shipment is active and a player pawn is within MaterializationRadius of its position. */
	UFUNCTION(BlueprintPure, Category = "Mining|Transport")
	bool ShouldMaterializeCourier() const;

	/** True between a SaveGame load and OnComponentLoaded; the ledger is frozen during that time. */
	UFUNCTION(BlueprintPure, Category = "Mining|Transport|Diagnostics")
	bool IsAwaitingRestore() const
	{
		return bAwaitingRestore;
	}

	/** The physical courier currently bound to the shipment, or null while it travels virtually. */
	UFUNCTION(BlueprintPure, Category = "Mining|Transport|Diagnostics")
	AACFCharacter* GetPhysicalCourier() const;

	/** Authority only. Reserves cargo from the mining site and starts an outbound shipment when none is active. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mining|Transport")
	bool TryDispatch();

	/** Authority only. Makes a living courier that stands near the shipment position, and whose controller is owned by this actor, the physical carrier of the shipment. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mining|Transport")
	bool BindPhysicalCourier(AACFCharacter* Courier, FGuid ShipmentId);

	/** Authority only. Returns the shipment to virtual travel at the courier's position; this is not a cargo loss. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mining|Transport")
	void UnbindPhysicalCourier(AACFCharacter* Courier);

	/** Called after the physical unload animation, never merely on starting travel. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mining|Transport")
	bool CompletePhysicalLeg(AACFCharacter* Courier, FGuid ShipmentId);

	/** Server broker records a real ACF escort death; culling must never call this. */
	bool RecordEscortLoss(FGuid ShipmentId, int32 Slot);

	/** Post-load hook: validates the restored shipment and resumes the ledger. Invalid or unknown-version data stays frozen. */
	UFUNCTION(BlueprintCallable, Category = "Mining|Timing")
	void OnComponentLoaded();

	/** Distance from the shipment position within which a player pawn causes the physical convoy to be spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Transport", meta = (ClampMin = "1.0", Units = "cm"))
	float MaterializationRadius = 2500.f;

private:
	friend class AMiningSiteActor;
	friend class UPangeaMiningConvoyComponent;
	void PrepareForOwnerRestore();
	UMiningSiteComponent* GetSite() const;
	bool HasAuthority() const;
	void AdvanceVirtual(double Seconds);
	void TryUnload();
	void Publish();
	UFUNCTION()
	void HandleCourierDeath(AACFCharacter* Courier);
	UPROPERTY(Replicated, SaveGame)
	FPangeaMiningShipment Shipment;
	UPROPERTY(SaveGame)
	int32 TransportVersion = 3;
	UPROPERTY(SaveGame)
	float RiskSample = 0.f;
	TWeakObjectPtr<AACFCharacter> PhysicalCourier;
	bool bAwaitingRestore = false;
	bool bOperationInProgress = false;
	bool bCourierDeathPending = false;
};
