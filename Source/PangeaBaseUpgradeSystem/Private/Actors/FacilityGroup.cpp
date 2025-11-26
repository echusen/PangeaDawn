#include "Actors/FacilityGroup.h"

#include "Animation/SkeletalMeshActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/FacilitySlotComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Character.h"


// ---------------------------------------------------------
// Constructor
// ---------------------------------------------------------
AFacilityGroup::AFacilityGroup()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

// ---------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------
void AFacilityGroup::BeginPlay()
{
    Super::BeginPlay();
    SpawnAllSlots();
}

// ---------------------------------------------------------
// Find all slot components & spawn actors for each
// ---------------------------------------------------------
void AFacilityGroup::SpawnAllSlots()
{
    TArray<UFacilitySlotComponent*> Slots;
    GetComponents<UFacilitySlotComponent>(Slots);

    for (UFacilitySlotComponent* Slot : Slots)
    {
        SpawnFromSlot(Slot);
    }
}

// ---------------------------------------------------------
// Core spawn function for each individual slot
// ---------------------------------------------------------
void AFacilityGroup::SpawnFromSlot(UFacilitySlotComponent* Slot)
{
    if (!Slot)
        return;

    UWorld* World = GetWorld();
    if (!World)
        return;

    // Base transform
    FVector SpawnLoc = Slot->GetComponentLocation();
    FRotator SpawnRot = Slot->GetComponentRotation();

    // -----------------------------------------------------
    // Snap to ground
    // -----------------------------------------------------
    if (Slot->bSnapToGround)
    {
        FHitResult Hit;
        FVector Start = SpawnLoc + FVector(0.f, 0.f, 500.f);
        FVector End = SpawnLoc - FVector(0.f, 0.f, 2000.f);

        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);

        if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
        {
            SpawnLoc = Hit.Location;
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AActor* Spawned = nullptr;

    // -----------------------------------------------------
    // PRIORITY 1: ActorClass overrides everything
    // NPCs, interactables, custom blueprints, etc.
    // -----------------------------------------------------
    if (Slot->ActorClass)
    {
        Spawned = World->SpawnActor<AActor>(
            Slot->ActorClass,
            SpawnLoc,
            SpawnRot,
            SpawnParams
        );
    }

    // -----------------------------------------------------
    // PRIORITY 2: Decoration via Static Mesh
    // -----------------------------------------------------
    if (!Spawned)
    {
        if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Slot->DecorationAsset))
        {
            AStaticMeshActor* MeshActor =
                World->SpawnActor<AStaticMeshActor>(SpawnLoc, SpawnRot, SpawnParams);

            if (MeshActor)
            {
                MeshActor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
                Spawned = MeshActor;
            }
        }
    }

    // -----------------------------------------------------
    // PRIORITY 3: Decoration via Skeletal Mesh
    // -----------------------------------------------------
    if (!Spawned)
    {
        if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Slot->DecorationAsset))
        {
            ASkeletalMeshActor* SkelActor =
                World->SpawnActor<ASkeletalMeshActor>(SpawnLoc, SpawnRot, SpawnParams);

            if (SkelActor)
            {
                SkelActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkelMesh);
                SkelActor->GetSkeletalMeshComponent()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
                Spawned = SkelActor;
            }
        }
    }

    // -----------------------------------------------------
    // Nothing spawned — invalid slot
    // -----------------------------------------------------
    if (!Spawned)
        return;

    // -----------------------------------------------------
    // Apply ground-adjusted location
    // -----------------------------------------------------
    // Final position correction for characters
    if (ACharacter* Character = Cast<ACharacter>(Spawned))
    {
        UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
        if (Capsule)
        {
            float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
            FVector CorrectedLoc = SpawnLoc;
            CorrectedLoc.Z += HalfHeight;   // Raise character so feet match ground
            Spawned->SetActorLocation(CorrectedLoc);
        }
    }
    else
    {
        // Non-character actors stay as-is
        Spawned->SetActorLocation(SpawnLoc);
    }

    // -----------------------------------------------------
    // Attach to FacilityGroup
    // -----------------------------------------------------
    Spawned->AttachToComponent(
        RootComponent,
        FAttachmentTransformRules::KeepWorldTransform
    );

    // -----------------------------------------------------
    // Hide / disable if not unlocked yet
    // -----------------------------------------------------
    if (Slot->bHideUntilUnlocked)
    {
        Spawned->SetActorHiddenInGame(true);
        Spawned->SetActorEnableCollision(false);
        Spawned->SetActorTickEnabled(false);
    }

    // -----------------------------------------------------
    // Track spawned actor for enabling/disabling
    // -----------------------------------------------------
    SpawnedActors.Add(Spawned);
}

// ---------------------------------------------------------
// Called by FacilityManager to show or hide the actors
// ---------------------------------------------------------
void AFacilityGroup::SetFacilityEnabled(bool bEnabled)
{
    for (AActor* Actor : SpawnedActors)
    {
        if (!Actor) continue;

        Actor->SetActorHiddenInGame(!bEnabled);
        Actor->SetActorEnableCollision(bEnabled);
        Actor->SetActorTickEnabled(bEnabled);
    }
}
