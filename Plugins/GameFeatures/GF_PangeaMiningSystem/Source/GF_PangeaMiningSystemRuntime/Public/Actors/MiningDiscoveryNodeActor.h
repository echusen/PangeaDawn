#pragma once

#include "CoreMinimal.h"
#include "ALSSavableInterface.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interfaces/ACFInteractableInterface.h"
#include "MiningDiscoveryNodeActor.generated.h"

class AMiningSiteActor;
class FLifetimeProperty;
class UACFInteractionComponent;
class UMiningSiteDefinition;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<UStaticMeshComponent> NodeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<UMiningSiteDefinition> SiteDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining")
	TSubclassOf<AMiningSiteActor> MiningSiteActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining")
	FTransform SiteSpawnTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category="Mining|Unlock")
	bool bUnlocked = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Unlock")
	FGameplayTag FacilityTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Mining")
	TObjectPtr<AMiningSiteActor> EstablishedSite;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, SaveGame, Category="Mining")
	TObjectPtr<AActor> SettlementResourceActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	FText InteractionText;

	UFUNCTION(BlueprintPure, Category="Mining")
	bool CanSetUpSite() const;

	UFUNCTION(BlueprintCallable, Category="Mining|Unlock")
	void SetUnlocked(bool bInUnlocked);

	UFUNCTION(BlueprintCallable, Category="Mining")
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
	void HandleInteractionBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void HandleInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
