#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"

#include "ALSSavableInterface.h"
#include "Interfaces/ACFInteractableInterface.h"
#include "Items/ACFItem.h"

#include "MiningSettlementStockpileActor.generated.h"

class UACFInteractionComponent;
class UACFStorageComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UUserWidget;

/**
 * Settlement-side storage for mined goods. It owns the storage component that
 * holds the settlement stock, gives couriers a location to unload at, and lets
 * a player open the stockpile screen to move items between the stockpile and
 * their own inventory.
 */
UCLASS(BlueprintType, Blueprintable)
class GF_PANGEAMININGSYSTEMRUNTIME_API AMiningSettlementStockpileActor : public AActor, public IACFInteractableInterface, public IALSSavableInterface
{
	GENERATED_BODY()

public:
	AMiningSettlementStockpileActor();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual TArray<UActorComponent*> GetComponentsToSave_Implementation() const override;
	virtual void OnLoaded_Implementation() override;

protected:
	virtual void BeginPlay() override;

	/** Root component that the other stockpile components attach to. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Visible mesh of the stockpile. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<UStaticMeshComponent> StockpileMesh;

	/** Text label that shows the stored item count. It is hidden when play begins. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<UTextRenderComponent> StockpileLabel;

	/** Widget class for the stockpile menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|UI")
	TSubclassOf<UUserWidget> StockpileWidgetClass;

	/** Marker whose world location couriers use as their unload point. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<USceneComponent> CourierUnloadMarker;

public:
	/** Storage that holds the settlement stock. It is the only component of this actor written to the save game. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<UACFStorageComponent> StorageComponent;

	/** Items added to the storage on the authority when play begins and the storage is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining")
	TArray<FBaseItem> InitialStock;

	/** Opens the stockpile screen for the local player that controls InteractingPawn. Closes a screen that is already open first. */
	UFUNCTION(BlueprintCallable, Category = "Mining|UI")
	void OpenStockpileMenu(APawn* InteractingPawn);

	/** Drops the reference to the open stockpile widget when Widget is null or is that widget. */
	void ClearActiveStockpileWidget(UUserWidget* Widget);

	/** Server RPC: moves every item in the stockpile into the pawn's inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Mining|Storage")
	void ServerWithdrawAllToPawn(APawn* InteractingPawn);

	/** Server RPC: moves every item in the pawn's inventory into the stockpile. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Mining|Storage")
	void ServerDepositAllFromPawn(APawn* InteractingPawn);

	/** Server RPC: moves up to Count items of the first stockpile stack of ItemClass into the pawn's inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Mining|Storage")
	void ServerTransferItemToPawn(APawn* InteractingPawn, TSubclassOf<UACFItem> ItemClass, int32 Count);

	/** Server RPC: moves up to Count items of the pawn's first inventory stack of ItemClass into the stockpile. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Mining|Storage")
	void ServerTransferItemFromPawn(APawn* InteractingPawn, TSubclassOf<UACFItem> ItemClass, int32 Count);

	/** Returns the world location where couriers unload. Falls back to the actor location when the marker is missing. */
	FVector GetCourierUnloadLocation() const;

	virtual void OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnInteractableRegisteredByPawn_Implementation(APawn* Pawn) override;
	virtual void OnInteractableUnregisteredByPawn_Implementation(APawn* Pawn) override;
	virtual FText GetInteractableName_Implementation() override;
	virtual bool CanBeInteracted_Implementation(APawn* Pawn) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<USphereComponent> InteractionSphere;

	TWeakObjectPtr<UUserWidget> ActiveStockpileWidget;

	TArray<TWeakObjectPtr<UACFInteractionComponent>> RegisteredInteractionComponents;

	UFUNCTION()
	void UpdateStatusText();
	void RefreshLocalInteractionRegistration();
	void RegisterWithInteractionComponent(UACFInteractionComponent* InteractionComponent);
	void UnregisterFromInteractionComponent(UACFInteractionComponent* InteractionComponent);
	UFUNCTION()
	void HandleInteractionBegin(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	UFUNCTION()
	void HandleInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
