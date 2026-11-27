#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ACFInteractableInterface.h"
#include "Types/HuntingTypes.h"
#include "PangeaHuntClueActor.generated.h"

class UACFInteractionComponent;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Replicated world actor for a special hunting clue (blood, broken foliage and similar).
 * Spawned by the track emitter on the server, hidden until the local hunter sense reveals
 * it, and interactable through ACF so a player can identify the creature that left it.
 */
UCLASS()
class GF_PANGEAHUNTINGSYSTEMRUNTIME_API APangeaHuntClueActor : public AActor, public IACFInteractableInterface
{
	GENERATED_BODY()

public:
	APangeaHuntClueActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Copies the clue data from the track point, moves the actor there and starts its lifespan. */
	void InitializeClue(const FHuntTrackPoint& InTrackPoint, const FString& InSourceCreatureName);
	void SetBloodDecalMaterial(UMaterialInterface* InMaterial);
	void SetBloodDecalSize(const FVector& InDecalSize);
	void SetBrokenFoliageDecalMaterial(UMaterialInterface* InMaterial);
	void SetBrokenFoliageDecalSize(const FVector& InDecalSize);
	void SetGenericDecalMaterial(UMaterialInterface* InMaterial);
	void SetGenericDecalSize(const FVector& InDecalSize);

	/** Sets the interaction radius, never smaller than the decal of the current clue type. */
	void SetInteractionRadius(float InInteractionRadius);

	/** Shows or hides the clue for the local player and registers it for local interaction while shown. */
	void SetRevealState(bool bRevealed, float VisibilityAlpha);

	float GetCreatedServerTime() const
	{
		return CreatedServerTime;
	}

	float GetLifetime() const
	{
		return Lifetime;
	}

	EHuntClueType GetClueType() const
	{
		return ClueType;
	}

	/** True once a pawn has interacted with this clue. */
	UFUNCTION(BlueprintPure, Category = "Hunting")
	bool IsClueIdentified() const
	{
		return bClueIdentified;
	}

	virtual void OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual void OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = "") override;
	virtual FText GetInteractableName_Implementation() override;
	virtual bool CanBeInteracted_Implementation(APawn* Pawn) override;

protected:
	/** Root of the actor. The mesh and the decal attach to it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hunting")
	TObjectPtr<USceneComponent> Root;

	/** Flat mesh shown only when the clue type is Footprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hunting")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Decal shown for every clue type except Footprint, not only for blood. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hunting")
	TObjectPtr<UDecalComponent> BloodDecal;

	/** Kind of clue this actor represents. Selects the decal, its size and the display name. */
	UPROPERTY(ReplicatedUsing = OnRep_ClueData, VisibleAnywhere, BlueprintReadOnly, Category = "Hunting")
	EHuntClueType ClueType = EHuntClueType::Custom;

	/** Name of the creature that left the clue. */
	UPROPERTY(ReplicatedUsing = OnRep_ClueData, VisibleAnywhere, BlueprintReadOnly, Category = "Hunting")
	FString SourceCreatureName;

	/** World time in seconds on the server when the clue was created. */
	UPROPERTY(ReplicatedUsing = OnRep_ClueData, VisibleAnywhere, BlueprintReadOnly, Category = "Hunting")
	float CreatedServerTime = 0.f;

	/** Seconds the clue stays in the world after creation. */
	UPROPERTY(ReplicatedUsing = OnRep_ClueData, VisibleAnywhere, BlueprintReadOnly, Category = "Hunting")
	float Lifetime = 0.f;

	/** Set once a pawn has interacted with this clue. */
	UPROPERTY(ReplicatedUsing = OnRep_ClueData, VisibleAnywhere, BlueprintReadOnly, Category = "Hunting")
	bool bClueIdentified = false;

	UFUNCTION()
	void OnRep_ClueData();

private:
	void ApplyVisualStyle();
	void RegisterForLocalInteraction(bool bShouldRegister);
	void MarkOwningTrackSetIdentified(APawn* Pawn);
	UStaticMesh* ResolveFallbackMesh() const;
	UMaterialInterface* ResolveFallbackMaterial() const;
	UMaterialInterface* ResolveFallbackBloodDecalMaterial() const;
	UMaterialInterface* ResolveFallbackBrokenFoliageDecalMaterial() const;
	UMaterialInterface* ResolveFallbackGenericDecalMaterial() const;
	FText ResolveClueDisplayName() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MeshMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DecalMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BloodDecalMaterialOverride;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BrokenFoliageDecalMaterialOverride;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> GenericDecalMaterialOverride;

	UPROPERTY(Transient)
	TWeakObjectPtr<UACFInteractionComponent> RegisteredInteractionComponent;

	FVector BloodDecalSize = FVector(16.f, 72.f, 72.f);
	FVector BrokenFoliageDecalSize = FVector(16.f, 96.f, 96.f);
	FVector GenericDecalSize = FVector(16.f, 72.f, 72.f);
	float InteractionRadius = 88.f;

	bool bIsRevealedLocally = false;
	bool bRegisteredForLocalInteraction = false;
};
