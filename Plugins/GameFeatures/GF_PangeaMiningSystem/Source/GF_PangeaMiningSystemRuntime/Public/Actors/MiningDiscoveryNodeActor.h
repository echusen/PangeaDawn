#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "ALSSavableInterface.h"
#include "Interfaces/ACFInteractableInterface.h"

#include "MiningDiscoveryNodeActor.generated.h"

class AMiningSiteActor;
class FLifetimeProperty;
class UACFInteractionComponent;
class UMiningSiteDefinition;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * World-placed node that marks a spot where a mining site can be founded.
 * While it is unlocked and has no site yet, interacting with it spawns the
 * configured mining site actor and establishes it. Once a site exists the
 * node no longer offers an interaction.
 */
UCLASS(BlueprintType, Blueprintable)
class GF_PANGEAMININGSYSTEMRUNTIME_API AMiningDiscoveryNodeActor : public AActor, public IACFInteractableInterface, public IALSSavableInterface
{
	GENERATED_BODY()

public:
	AMiningDiscoveryNodeActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual TArray<UActorComponent*> GetComponentsToSave_Implementation() const override;
	virtual void OnLoaded_Implementation() override;

	/** Root component that the node mesh and the interaction sphere attach to. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Visible mesh of the node in the world. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<UStaticMeshComponent> NodeMesh;

	/** Site definition assigned to the mining site that this node spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<UMiningSiteDefinition> SiteDefinition;

	/** Mining site actor class spawned by SetUpSite. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining")
	TSubclassOf<AMiningSiteActor> MiningSiteActorClass;

	/** Spawn transform of the mining site, relative to this node's actor transform. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining")
	FTransform SiteSpawnTransform;

	/** Whether a site may be set up from this node. Stored in the save game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining|Unlock")
	bool bUnlocked = true;

	/** Facility tag that identifies this node for unlocking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Unlock")
	FGameplayTag FacilityTag;

	/** Mining site spawned from this node, or null while no site has been established. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Mining")
	TObjectPtr<AMiningSiteActor> EstablishedSite;

	/** Settlement actor passed to the spawned site as its settlement resource actor. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, SaveGame, Category = "Mining")
	TObjectPtr<AActor> SettlementResourceActor;

	/** Interaction prompt shown while a site can be set up from this node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractionText;

	/** Returns true when the node is unlocked, has no established site, and has both a site definition and a site actor class. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	bool CanSetUpSite() const;

	/** Sets the unlocked state. On a change it refreshes the interaction registration of nearby pawns. */
	UFUNCTION(BlueprintCallable, Category = "Mining|Unlock")
	void SetUnlocked(bool bInUnlocked);

	/**
	 * Returns the established site when one already exists. Otherwise, on the authority, spawns the
	 * mining site actor and establishes it. Returns null when the site cannot be set up.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	AMiningSiteActor* SetUpSite();

	virtual void OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnInteractableRegisteredByPawn_Implementation(APawn* Pawn) override;
	virtual void OnInteractableUnregisteredByPawn_Implementation(APawn* Pawn) override;
	virtual FText GetInteractableName_Implementation() override;
	virtual bool CanBeInteracted_Implementation(APawn* Pawn) override;

protected:
	virtual void BeginPlay() override;

private:
	TObjectPtr<USphereComponent> InteractionSphere;

	TArray<TWeakObjectPtr<UACFInteractionComponent>> RegisteredInteractionComponents;

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
