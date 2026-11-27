#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "PangeaMiningConvoyComponent.generated.h"

class AACFCharacter;
class AActor;
class UNavigationInvokerComponent;
class UPangeaMiningTransportComponent;

/** Server-only physical presentation broker. Cargo remains exclusively in the transport ledger. */
UCLASS(ClassGroup = (Pangea), BlueprintType, meta = (BlueprintSpawnableComponent))
class GF_PANGEAMININGSYSTEMRUNTIME_API UPangeaMiningConvoyComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPangeaMiningConvoyComponent();

	/** Server only: spawns the physical convoy when the active shipment should be visible and removes it when it should not. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The spawned courier character of the current convoy, or null while the shipment is virtual. */
	UFUNCTION(BlueprintPure, Category = "Presentation")
	AACFCharacter* GetCourier() const;

	/** The currently valid spawned escort characters of the current convoy. */
	UFUNCTION(BlueprintPure, Category = "Presentation")
	TArray<AACFCharacter*> GetEscorts() const;

	/** The spawned cosmetic cart actor, or null when the convoy has none. */
	UFUNCTION(BlueprintPure, Category = "Presentation")
	AActor* GetCart() const;

	/** The spawned draft animal that tows the cart, or null when the convoy has none. */
	UFUNCTION(BlueprintPure, Category = "Presentation")
	AACFCharacter* GetDraftAnimal() const;

	/** Complete physical unloading/return waits for the animal, not just its handler. */
	UFUNCTION(BlueprintPure, Category = "Diagnostics")
	bool IsDraftReadyForArrival(const FVector& Goal) const;

	/** Escort slot value that identifies the draft animal instead of a courier (INDEX_NONE) or an escort (0 and up). */
	static constexpr int32 DraftAnimalSlot = -2;

	/** Why the physical convoy last failed to materialize or was paused; empty after a successful materialization. */
	UFUNCTION(BlueprintPure, Category = "Diagnostics")
	FString GetBlockedReason() const
	{
		return BlockedReason;
	}

	/** Authority only. Unbinds the courier from the transport ledger and retires every spawned convoy actor; the shipment's cargo stays in the ledger. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Presentation")
	void ClearPhysicalConvoy();

	/** Players must be farther than this from every convoy member before the physical convoy is removed. Never less than the transport's materialization radius plus 100 cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "1.0", Units = "cm"))
	float DematerializationRadius = 3000.f;

	/** Cosmetic cart attachment only; not a mount, hitch, vehicle or economic container. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FTransform CartRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(-220, 0, -90));

	/** Kinematic tow offset relative to the animal's capsule; no wheel/hitch physics. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FTransform DraftCartRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(-400, 0, -105));

	/** The transport ledger component on the same owner, or null. */
	UPangeaMiningTransportComponent* GetTransport() const;

private:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool Materialize();
	bool CanDematerialize() const;
	AACFCharacter* SpawnMember(UClass* Class, const FVector& GroundLocation, int32 EscortSlot);
	void RetireMember(AACFCharacter* Member);
	UFUNCTION()
	void HandleEscortDeath(AACFCharacter* Escort);
	UFUNCTION()
	void HandleDraftAnimalDeath(AACFCharacter* Animal);
	UPROPERTY(Transient)
	TObjectPtr<AACFCharacter> Courier;
	UPROPERTY(Transient)
	TObjectPtr<AACFCharacter> DraftAnimal;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AACFCharacter>> Escorts;
	UPROPERTY(Transient)
	TObjectPtr<AActor> Cart;
	UPROPERTY(Transient)
	TObjectPtr<UNavigationInvokerComponent> NavigationInvoker;
	FGuid ManifestId;
	FString BlockedReason;
	double RetryAt = 0.;
	bool bClearing = false;
};
