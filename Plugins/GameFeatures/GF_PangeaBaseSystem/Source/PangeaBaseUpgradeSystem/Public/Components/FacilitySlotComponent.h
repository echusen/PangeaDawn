// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Types/FacilityTypes.h"
#include "FacilitySlotComponent.generated.h"

class UChildActorComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FPropertyChangedEvent;

/**
 * Scene component that marks where a facility group spawns one actor or mesh.
 * Designers place slots on a facility group; the group reads them at BeginPlay. In the editor the slot shows a preview.
 */
UCLASS(ClassGroup = (Facility), meta = (BlueprintSpawnableComponent))
class PANGEABASEUPGRADESYSTEM_API UFacilitySlotComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFacilitySlotComponent();
	virtual void BeginPlay() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** What this slot spawns. Decides which of the asset properties below is editable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Slot")
	EFacilitySlotType SlotType = EFacilitySlotType::Decoration;

	/*
		ActorClass: Used for NPC, Interactable, or Custom slots.
		Not visible when SlotType == Decoration.
	*/
	UPROPERTY(EditAnywhere,
		BlueprintReadOnly,
		Category = "NPC/Interactable",
		meta = (EditCondition = "SlotType == EFacilitySlotType::NPC || SlotType == EFacilitySlotType::Interactable || SlotType == EFacilitySlotType::Custom"))
	TSubclassOf<AActor> ActorClass;

	/*
		DecorationAsset: Only visible when SlotType == Decoration.
		Can be StaticMesh or SkeletalMesh.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration", meta = (EditCondition = "SlotType == EFacilitySlotType::Decoration"))
	TObjectPtr<UObject> DecorationAsset = nullptr;

	/*
		Hide until the facility unlocks (any slot type)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Slot")
	bool bHideUntilUnlocked = true;

	/*
		Snap spawned actors to ground
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Slot")
	bool bSnapToGround = true;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	TObjectPtr<UChildActorComponent> EditorActorPreview;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> EditorStaticPreview;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> EditorSkeletalPreview;

#endif
};
