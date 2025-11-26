// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/FacilitySlotComponent.h"
#include "Components/StaticMeshComponent.h"


UFacilitySlotComponent::UFacilitySlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
#if WITH_EDITORONLY_DATA

	// Actor preview (NPC / Interactable)
	EditorActorPreview = CreateDefaultSubobject<UChildActorComponent>(TEXT("ActorPreview"));
	EditorActorPreview->SetupAttachment(this);
	EditorActorPreview->SetVisibility(true);
	EditorActorPreview->SetMobility(EComponentMobility::Movable);
	EditorActorPreview->bHiddenInGame = true;

	// Static mesh preview (Decoration)
	EditorStaticPreview = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticPreview"));
	EditorStaticPreview->SetupAttachment(this);
	EditorStaticPreview->SetVisibility(false);
	EditorStaticPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EditorStaticPreview->bHiddenInGame = true;

	// Skeletal mesh preview (Decoration)
	EditorSkeletalPreview = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalPreview"));
	EditorSkeletalPreview->SetupAttachment(this);
	EditorSkeletalPreview->SetVisibility(false);
	EditorSkeletalPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EditorSkeletalPreview->bHiddenInGame = true;

#endif
}

void UFacilitySlotComponent::BeginPlay()
{
	Super::BeginPlay();
	
#if WITH_EDITORONLY_DATA
	if (EditorActorPreview) EditorActorPreview->DestroyComponent();
	if (EditorStaticPreview) EditorStaticPreview->DestroyComponent();
	if (EditorSkeletalPreview) EditorSkeletalPreview->DestroyComponent();
#endif
}

#if WITH_EDITOR

void UFacilitySlotComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Hide all first
	if (EditorActorPreview) EditorActorPreview->SetVisibility(false);
	if (EditorStaticPreview) EditorStaticPreview->SetVisibility(false);
	if (EditorSkeletalPreview) EditorSkeletalPreview->SetVisibility(false);

	// NPC / Interactable / Custom = Use ActorClass
	if (SlotType != EFacilitySlotType::Decoration && ActorClass)
	{
		if (EditorActorPreview)
		{
			EditorActorPreview->SetChildActorClass(ActorClass);
			EditorActorPreview->SetVisibility(true);
		}
		return;
	}

	// Decoration = Static Mesh?
	if (SlotType == EFacilitySlotType::Decoration)
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(DecorationAsset))
		{
			if (EditorStaticPreview)
			{
				EditorStaticPreview->SetStaticMesh(StaticMesh);
				EditorStaticPreview->SetVisibility(true);
			}
			return;
		}

		// Decoration = Skeletal Mesh?
		if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(DecorationAsset))
		{
			if (EditorSkeletalPreview)
			{
				EditorSkeletalPreview->SetSkeletalMesh(SkelMesh);
				EditorSkeletalPreview->SetVisibility(true);
			}
			return;
		}
	}
}

#endif

