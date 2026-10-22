#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/MiningTypes.h"
#include "PangeaMiningTransportComponent.generated.h"

class AACFCharacter;
class UMiningSiteComponent;

UENUM(BlueprintType)
enum class EPangeaMiningShipmentPhase : uint8 { Idle, Outbound, AwaitingUnload, Returning };

/** The authoritative cargo ledger. Presentation actors never own or duplicate its items. */
USTRUCT(BlueprintType)
struct GF_PANGEAMININGSYSTEMRUNTIME_API FPangeaMiningShipment
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	FGuid Id;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	EPangeaMiningShipmentPhase Phase = EPangeaMiningShipmentPhase::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	FMiningStoredItem Cargo;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	TObjectPtr<AActor> DestinationActor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	FVector Origin = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	FVector Destination = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	FVector Position = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	float Speed = 320.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	float LossChance = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	double VirtualTravelSeconds = 0.0;
	UPROPERTY(SaveGame)
	double PlannedOutboundSeconds = 1.0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	bool bRiskResolved = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	bool bLost = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	int32 DeliveredUnits = 0;
	/** Convoy configuration is fixed for this manifest, not changed mid-trip by upgrades. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	TSoftClassPtr<AActor> CourierClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	TSoftClassPtr<AActor> EscortClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	TSoftClassPtr<AActor> ChariotClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	TSoftClassPtr<AActor> DraftAnimalClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	int32 EscortCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Transport")
	TArray<int32> LostEscortSlots;
};

/** Hybrid logistics foundation. Enable only after the ACF physical-actor broker is configured. */
UCLASS(ClassGroup=(Pangea), BlueprintType, meta=(BlueprintSpawnableComponent))
class GF_PANGEAMININGSYSTEMRUNTIME_API UPangeaMiningTransportComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPangeaMiningTransportComponent();
	virtual void Serialize(FArchive& Ar) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Mining|Transport")
	bool IsHybridEnabled() const;
	UFUNCTION(BlueprintPure, Category="Mining|Transport")
	bool HasActiveShipment() const { return Shipment.Phase != EPangeaMiningShipmentPhase::Idle; }
	UFUNCTION(BlueprintPure, Category="Mining|Transport")
	FPangeaMiningShipment GetShipment() const { return Shipment; }
	UFUNCTION(BlueprintPure, Category="Mining|Transport")
	bool ShouldMaterializeCourier() const;
	UFUNCTION(BlueprintPure, Category="Mining|Transport|Diagnostics")
	bool IsAwaitingRestore() const { return bAwaitingRestore; }
	UFUNCTION(BlueprintPure, Category="Mining|Transport|Diagnostics")
	AACFCharacter* GetPhysicalCourier() const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining|Transport")
	bool TryDispatch();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining|Transport")
	bool BindPhysicalCourier(AACFCharacter* Courier, FGuid ShipmentId);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining|Transport")
	void UnbindPhysicalCourier(AACFCharacter* Courier);
	/** Called after the physical unload animation, never merely on starting travel. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining|Transport")
	bool CompletePhysicalLeg(AACFCharacter* Courier, FGuid ShipmentId);
	/** Server broker records a real ACF escort death; culling must never call this. */
	bool RecordEscortLoss(FGuid ShipmentId, int32 Slot);
	UFUNCTION(BlueprintCallable, Category="Mining|Timing")
	void OnComponentLoaded();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Transport", meta=(ClampMin="1.0", Units="cm"))
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
