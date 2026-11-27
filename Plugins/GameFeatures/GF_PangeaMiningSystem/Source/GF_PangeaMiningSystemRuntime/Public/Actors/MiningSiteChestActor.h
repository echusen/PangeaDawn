#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"

#include "Interfaces/ACFInteractableInterface.h"

#include "MiningSiteChestActor.generated.h"

class UACFInteractionComponent;
class UACFStorageComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UUserWidget;

/**
 * Storage chest of a mining site. It owns the storage component that holds
 * the site's items and lets a player open the chest screen or withdraw the
 * whole content into their inventory.
 */
UCLASS(BlueprintType, Blueprintable)
class GF_PANGEAMININGSYSTEMRUNTIME_API AMiningSiteChestActor : public AActor, public IACFInteractableInterface
{
	GENERATED_BODY()

public:
	AMiningSiteChestActor();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Root component that the other chest components attach to. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Visible mesh of the chest. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<UStaticMeshComponent> ChestMesh;

	/** Storage that holds the items kept in this chest. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<UACFStorageComponent> StorageComponent;

	/** Widget class for the chest menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|UI")
	TSubclassOf<UUserWidget> ChestWidgetClass;

	/** Opens the chest screen for the local player that controls InteractingPawn. Closes a screen that is already open first. */
	UFUNCTION(BlueprintCallable, Category = "Mining|UI")
	void OpenChestMenu(APawn* InteractingPawn);

	/** Drops the reference to the open chest widget when Widget is null or is that widget. */
	void ClearActiveChestWidget(UUserWidget* Widget);

	/** Server RPC: moves every item in the chest into the pawn's inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Mining|Storage")
	void ServerWithdrawAllToPawn(APawn* InteractingPawn);

	virtual void OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnInteractableRegisteredByPawn_Implementation(APawn* Pawn) override;
	virtual void OnInteractableUnregisteredByPawn_Implementation(APawn* Pawn) override;
	virtual FText GetInteractableName_Implementation() override;
	virtual bool CanBeInteracted_Implementation(APawn* Pawn) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> StatusText;

	TWeakObjectPtr<UUserWidget> ActiveChestWidget;

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
