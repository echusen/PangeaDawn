#pragma once

#include "CoreMinimal.h"
#include "ALSSavableInterface.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ACFInteractableInterface.h"
#include "SmartObjectRuntime.h"
#include "MiningSiteActor.generated.h"

class UACFInteractionComponent;
class UMiningSiteComponent;
class UActorComponent;
class USceneComponent;
class UUserWidget;
class USmartObjectComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class AACFCharacter;
class UPangeaMiningTransportComponent;
class UPangeaMiningConvoyComponent;
struct FMiningSiteLevelDefinition;
struct FTimerHandle;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<UMiningSiteComponent> MiningSiteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Transport")
	TObjectPtr<UPangeaMiningTransportComponent> TransportComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Transport")
	TObjectPtr<UPangeaMiningConvoyComponent> ConvoyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|AI")
	TObjectPtr<UActorComponent> PresentationCoordinator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<UStaticMeshComponent> SiteMarkerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Storage")
	TObjectPtr<USceneComponent> SiteChestMarker;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Replicated, SaveGame, Category="Mining")
	TObjectPtr<AActor> SettlementResourceActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|UI")
	TSubclassOf<UUserWidget> MiningSiteMenuClass;

	UFUNCTION(BlueprintCallable, Category="Mining")
	void RefreshLevelVisuals();

	UFUNCTION(BlueprintCallable, Category="Mining|UI")
	void OpenInteractionMenu(APawn* InteractingPawn);

	void ClearActiveMenuWidget(UUserWidget* Widget);

	UFUNCTION(BlueprintPure, Category="Mining")
	bool CanUpgradeFromInteraction(APawn* InteractingPawn) const;

	UFUNCTION(BlueprintPure, Category="Mining|Workforce")
	bool CanRecruitFromInteraction(APawn* InteractingPawn) const;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Mining")
	void ServerPurchaseNextUpgrade(APawn* InteractingPawn);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Mining")
	void ServerSyncProduction();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Mining")
	void ServerAdvanceOneSimulatedDay();

	virtual void OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnInteractableRegisteredByPawn_Implementation(APawn* Pawn) override;
	virtual void OnInteractableUnregisteredByPawn_Implementation(APawn* Pawn) override;
	virtual FText GetInteractableName_Implementation() override;
	virtual bool CanBeInteracted_Implementation(APawn* Pawn) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<AActor> SpawnedLevelVisuals;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Storage")
	TObjectPtr<AActor> SpawnedSiteChest;

	UPROPERTY(Transient)
	TObjectPtr<USphereComponent> SpawnedSiteChestInteractionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|AI")
	TArray<TObjectPtr<AActor>> SpawnedWorkerActors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|AI")
	TArray<TObjectPtr<AActor>> SpawnedGuardActors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|AI")
	TObjectPtr<AActor> SpawnedCourierActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|SmartObjects")
	TArray<TObjectPtr<USmartObjectComponent>> WorkerSmartObjectComponents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|SmartObjects")
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
	void HandleInteractionBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void HandleInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	UFUNCTION()
	void HandleSiteChestInteractionBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void HandleSiteChestInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
