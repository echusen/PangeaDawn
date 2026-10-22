#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PangeaMiningConvoyComponent.generated.h"

class AACFCharacter;
class UPangeaMiningTransportComponent;
class UNavigationInvokerComponent;

/** Server-only physical presentation broker. Cargo remains exclusively in the transport ledger. */
UCLASS(ClassGroup=(Pangea), BlueprintType, meta=(BlueprintSpawnableComponent))
class GF_PANGEAMININGSYSTEMRUNTIME_API UPangeaMiningConvoyComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPangeaMiningConvoyComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	UFUNCTION(BlueprintPure, Category="Presentation")
	AACFCharacter* GetCourier() const;
	UFUNCTION(BlueprintPure, Category="Presentation")
	TArray<AACFCharacter*> GetEscorts() const;
	UFUNCTION(BlueprintPure, Category="Presentation")
	AActor* GetCart() const;
	UFUNCTION(BlueprintPure, Category="Presentation")
	AACFCharacter* GetDraftAnimal() const;
	/** Complete physical unloading/return waits for the animal, not just its handler. */
	UFUNCTION(BlueprintPure, Category="Diagnostics")
	bool IsDraftReadyForArrival(const FVector& Goal) const;
	static constexpr int32 DraftAnimalSlot = -2;
	UFUNCTION(BlueprintPure, Category="Diagnostics")
	FString GetBlockedReason() const { return BlockedReason; }
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Presentation")
	void ClearPhysicalConvoy();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(ClampMin="1.0", Units="cm"))
	float DematerializationRadius = 3000.f;
	/** Cosmetic cart attachment only; not a mount, hitch, vehicle or economic container. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
	FTransform CartRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(-220, 0, -90));
	/** Kinematic tow offset relative to the animal's capsule; no wheel/hitch physics. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
	FTransform DraftCartRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(-400, 0, -105));
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
