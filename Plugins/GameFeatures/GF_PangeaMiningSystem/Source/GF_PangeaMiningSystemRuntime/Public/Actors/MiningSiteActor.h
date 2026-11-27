#pragma once

#include "CoreMinimal.h"

#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "SmartObjectRuntime.h"

#include "ALSSavableInterface.h"
#include "Interfaces/ACFInteractableInterface.h"

#include "MiningSiteActor.generated.h"

class AACFCharacter;
class FLifetimeProperty;
class UACFInteractionComponent;
class UActorComponent;
class UMiningSiteComponent;
class UPangeaMiningConvoyComponent;
class UPangeaMiningTransportComponent;
class USceneComponent;
class USmartObjectComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UUserWidget;
struct FMiningItemQuantity;
struct FMiningSiteLevelDefinition;
struct FTimerHandle;

/**
 * World actor of a mining site. It owns the mining site, transport and convoy
 * components, spawns the visual set and the site chest for the current site
 * level, hands the worker, guard and courier presentation to a presentation
 * coordinator component, and offers the upgrade and recruit interactions to
 * the player.
 */
UCLASS(BlueprintType, Blueprintable)
class GF_PANGEAMININGSYSTEMRUNTIME_API AMiningSiteActor : public AActor, public IACFInteractableInterface, public IALSSavableInterface
{
	GENERATED_BODY()

public:
	AMiningSiteActor();
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual TArray<UActorComponent*> GetComponentsToSave_Implementation() const override;
	virtual void OnLoaded_Implementation() override;

	/** Root component that the other site components attach to. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Mining site component of this site. It is written to the save game. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<UMiningSiteComponent> MiningSiteComponent;

	/** Transport component of this site. It is written to the save game. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Transport")
	TObjectPtr<UPangeaMiningTransportComponent> TransportComponent;

	/** Convoy component of this site. Its physical convoy is cleared when play ends. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Transport")
	TObjectPtr<UPangeaMiningConvoyComponent> ConvoyComponent;

	/** Component created on the authority at runtime that manages the worker, guard and courier presentation actors. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|AI")
	TObjectPtr<UActorComponent> PresentationCoordinator;

	/** Marker mesh of the site. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<UStaticMeshComponent> SiteMarkerMesh;

	/** Marker whose world transform is used to spawn the site chest. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Storage")
	TObjectPtr<USceneComponent> SiteChestMarker;

	/**
	 * Settlement actor whose storage receives delivered shipments. When set, it is also the cost context
	 * for upgrades and recruitment in place of the interacting pawn.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Replicated, SaveGame, Category = "Mining")
	TObjectPtr<AActor> SettlementResourceActor;

	/** Widget class for the mining site menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|UI")
	TSubclassOf<UUserWidget> MiningSiteMenuClass;

	/** On the authority of an established site, replaces the spawned visual set actor with the one for the current site level. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	void RefreshLevelVisuals();

	/** Opens the mining site screen for the local player that controls InteractingPawn. Closes a screen that is already open first. */
	UFUNCTION(BlueprintCallable, Category = "Mining|UI")
	void OpenInteractionMenu(APawn* InteractingPawn);

	/** Drops the reference to the open menu widget when Widget is null or is that widget. */
	void ClearActiveMenuWidget(UUserWidget* Widget);

	/** Returns true when the site is established, can be upgraded, and that upgrade is free or affordable for the cost context. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	bool CanUpgradeFromInteraction(APawn* InteractingPawn) const;

	/** Returns true when InteractingPawn is a player's possessed pawn within 600 units of the site and the site can recruit a worker. */
	UFUNCTION(BlueprintPure, Category = "Mining|Workforce")
	bool CanRecruitFromInteraction(APawn* InteractingPawn) const;

	/** Server RPC: purchases the next site upgrade with the cost context resolved for InteractingPawn. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Mining")
	void ServerPurchaseNextUpgrade(APawn* InteractingPawn);

	/** Server RPC: syncs production from world time and refreshes the status text. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Mining")
	void ServerSyncProduction();

	/** Server RPC: produces one simulated day of output, then tries an automatic shipment. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Mining")
	void ServerAdvanceOneSimulatedDay();

	virtual void OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnInteractableRegisteredByPawn_Implementation(APawn* Pawn) override;
	virtual void OnInteractableUnregisteredByPawn_Implementation(APawn* Pawn) override;
	virtual FText GetInteractableName_Implementation() override;
	virtual bool CanBeInteracted_Implementation(APawn* Pawn) override;

protected:
	virtual void BeginPlay() override;

	/** Visual set actor spawned for the current site level. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<AActor> SpawnedLevelVisuals;

	/** Chest actor spawned for this site. Its storage component is linked to the mining site component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Storage")
	TObjectPtr<AActor> SpawnedSiteChest;

	UPROPERTY(Transient)
	TObjectPtr<USphereComponent> SpawnedSiteChestInteractionSphere;

	/** Worker presentation actors tracked for this site. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|AI")
	TArray<TObjectPtr<AActor>> SpawnedWorkerActors;

	/** Guard presentation actors tracked for this site. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|AI")
	TArray<TObjectPtr<AActor>> SpawnedGuardActors;

	/** Courier presentation actor tracked for this site. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|AI")
	TObjectPtr<AActor> SpawnedCourierActor;

	/** Smart object components for workers, handed to the presentation coordinator. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|SmartObjects")
	TArray<TObjectPtr<USmartObjectComponent>> WorkerSmartObjectComponents;

	/** Smart object components for guards, handed to the presentation coordinator. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|SmartObjects")
	TArray<TObjectPtr<USmartObjectComponent>> GuardSmartObjectComponents;

	UFUNCTION()
	void HandleSiteLevelChanged(int32 OldLevel, int32 NewLevel);

	UFUNCTION()
	void HandleShipmentResolved(const FMiningItemQuantity& RequestedShipment, int32 DeliveredQuantity, bool bLost);

	UFUNCTION()
	void HandleWorkerDeath(AACFCharacter* Worker);

	UFUNCTION()
	void HandleMiningDayEnded(int64 CompletedDay, int32 LostWorkers);

	UFUNCTION()
	void HandleWorkforceChanged();

private:
	TObjectPtr<USphereComponent> InteractionSphere;
	TObjectPtr<UTextRenderComponent> StatusText;
	TArray<TWeakObjectPtr<UACFInteractionComponent>> RegisteredInteractionComponents;
	TWeakObjectPtr<UUserWidget> ActiveMenuWidget;
	FTimerHandle PresentationRefreshTimerHandle;
	FTimerHandle PresentationMovementTimerHandle;

	UFUNCTION()
	void UpdateStatusText();
	void RefreshSiteChest();
	void DestroySiteChest();
	void ClearSpawnedSiteChestInventory();
	void DestroyOwnedPresentationActors();
	void ResolveSettlementResourceActor();
	void ConfigureSmartObjectComponents();
	void RefreshPresentationActors();
	void ClearPresentationActors();
	void UpdatePresentationActorMovement();
	void EnsurePresentationCoordinator();
	void RefreshLocalInteractionRegistration();
	void RegisterWithInteractionComponent(UACFInteractionComponent* InteractionComponent);
	void UnregisterFromInteractionComponent(UACFInteractionComponent* InteractionComponent);
	void RegisterChestWithInteractionComponent(UACFInteractionComponent* InteractionComponent);
	void UnregisterChestFromInteractionComponent(UACFInteractionComponent* InteractionComponent);
	UObject* ResolveUpgradeCostContext(APawn* InteractingPawn) const;
	UFUNCTION()
	void HandleInteractionBegin(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	UFUNCTION()
	void HandleInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	UFUNCTION()
	void HandleSiteChestInteractionBegin(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	UFUNCTION()
	void HandleSiteChestInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
