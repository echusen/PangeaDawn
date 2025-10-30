// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.


#include "Actors/PangeaEggActor.h"

#include "Data/PangeaSpeciesDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "Objects/PangeaGeneticStrategy.h"

APangeaEggActor::APangeaEggActor()
{
    bReplicates = true;
}

void APangeaEggActor::BeginPlay()
{
    Super::BeginPlay();
}

void APangeaEggActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(APangeaEggActor, SpeciesData);
    DOREPLIFETIME(APangeaEggActor, ParentA);
    DOREPLIFETIME(APangeaEggActor, ParentB);
    DOREPLIFETIME(APangeaEggActor, ChildTraits);
}

void APangeaEggActor::InitializeEgg(const FParentSnapshot& InA, const FParentSnapshot& InB, UPangeaGeneticStrategy* Strategy, UPangeaSpeciesDataAsset* InSpecies)
{
    check(HasAuthority());

    ParentA = InA;
    ParentB = InB;
    
    SpeciesData = (InA.SpeciesID == InB.SpeciesID) ? InSpecies : nullptr;
    if (!SpeciesData)
    {
        UE_LOG(LogTemp, Error, TEXT("AEggActor::InitializeEgg — SpeciesData is null, cannot proceed"));
        return;
    }


    if (Strategy)
    {
        ChildTraits = Strategy->CombineTraits(ParentA, ParentB);
    }

    UE_LOG(LogTemp, Warning, TEXT("AEggActor::InitializeEgg — Egg initialized with species %s"), *SpeciesData->SpeciesID.ToString());

    StartIncubation();
}

void APangeaEggActor::StartIncubation()
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("AEggActor::StartIncubation skipped (no authority) for %s"), *GetName());
        return;
    }

    if (!SpeciesData)
    {
        UE_LOG(LogTemp, Error, TEXT("AEggActor::StartIncubation failed — SpeciesData is null for egg %s"), *GetName());
        return;
    }

    TotalIncubationTime = FMath::Max(SpeciesData->Incubation.IncubationSeconds, 0.1f);
    CurrentIncubationTime = 0.0f;

    UE_LOG(LogTemp, Warning, TEXT("AEggActor::StartIncubation — Starting incubation for %s (%.2f seconds)"), 
        *GetName(), TotalIncubationTime);

    if (UWorld* World = GetWorld())
    {
        
        World->GetTimerManager().SetTimer(
            IncubationTickHandle,
            this,
            &APangeaEggActor::TickIncubation,
            0.1f, // every 0.1s for smooth progress
            true
        );
        
        World->GetTimerManager().SetTimer(
            HatchTimerHandle,
            this,
            &APangeaEggActor::FinishIncubation,
            TotalIncubationTime,
            false
        );
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("AEggActor::StartIncubation — GetWorld() returned null for %s"), *GetName());
    }
}

void APangeaEggActor::TickIncubation()
{
    if (!TotalIncubationTime)
        return;

    CurrentIncubationTime += 0.1f;

    const float Progress = FMath::Clamp(CurrentIncubationTime / TotalIncubationTime, 0.f, 1.f);
    OnEggProgress.Broadcast(Progress);

    // Debug log every second for clarity
    if (FMath::Fmod(CurrentIncubationTime, 1.0f) < 0.11f)
    {
        UE_LOG(LogTemp, Verbose, TEXT("%s incubation progress: %.0f%%"), *GetName(), Progress * 100.f);
    }
}

void APangeaEggActor::FinishIncubation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(IncubationTickHandle);
        World->GetTimerManager().ClearTimer(HatchTimerHandle);
    }

    OnEggProgress.Broadcast(1.0f);
    UE_LOG(LogTemp, Warning, TEXT("Egg finished incubating: %s"), *GetName());
    Hatch();
}

AActor* APangeaEggActor::Hatch()
{
    if (!HasAuthority()) return nullptr;

    if (!SpeciesData || !SpeciesData->CreatureClass)
    {
        Destroy();
        return nullptr;
    }

    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AActor* NewCreature = GetWorld()->SpawnActor<AActor>(SpeciesData->CreatureClass, GetActorLocation(), GetActorRotation(), Params);
    if (NewCreature)
    {
        ApplyVisualInheritance(NewCreature); // Apply visual traits from parents
        OnEggHatched.Broadcast(NewCreature);
    }

    Destroy();
    return NewCreature;
}

void APangeaEggActor::ApplyVisualInheritance(AActor* NewCreature)
{
    if (!ValidateVisualInheritance(NewCreature))
        return;

    USkeletalMeshComponent* Mesh = NewCreature->FindComponentByClass<USkeletalMeshComponent>();
    const int32 SlotIndex = SpeciesData->InheritanceMaterialSlot;

    UMaterialInstanceDynamic* MID = CreateDynamicMaterial(NewCreature, SlotIndex);
    if (!MID) return;

    UE_LOG(LogTemp, Warning, TEXT("Applying visual inheritance to %s on slot %d"), *NewCreature->GetName(), SlotIndex);

    for (const FMaterialGeneticGroup& Group : SpeciesData->MaterialGeneticGroups)
    {
        ApplyMaterialGroupInheritance(MID, Group);
    }
}

bool APangeaEggActor::ValidateVisualInheritance(AActor* NewCreature) const
{
    if (!SpeciesData || !SpeciesData->bInheritParentMaterials)
    {
        UE_LOG(LogTemp, Warning, TEXT("AEggActor::ApplyVisualInheritance — Missing or disabled SpeciesData"));
        return false;
    }

    if (ParentA.VisualData.Num() == 0 || ParentB.VisualData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("AEggActor::ApplyVisualInheritance — Missing parent visual data"));
        return false;
    }

    if (!NewCreature || !NewCreature->FindComponentByClass<USkeletalMeshComponent>())
    {
        UE_LOG(LogTemp, Warning, TEXT("AEggActor::ApplyVisualInheritance — Missing mesh"));
        return false;
    }

    return true;
}

UMaterialInstanceDynamic* APangeaEggActor::CreateDynamicMaterial(AActor* NewCreature, int32 SlotIndex)
{
    USkeletalMeshComponent* Mesh = NewCreature->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh || SlotIndex >= Mesh->GetNumMaterials())
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid material slot %d for %s"), SlotIndex, *NewCreature->GetName());
        return nullptr;
    }

    UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(SlotIndex);
    if (!MID)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to create MID on %s"), *NewCreature->GetName());
    }

    return MID;
}

FLinearColor APangeaEggActor::MixParentColors(const FLinearColor& ColorA, const FLinearColor& ColorB) const
{
    const float InheritBias = FMath::FRandRange(0.3f, 0.7f);
    const float CurvedBlend = FMath::Pow(InheritBias, 0.8f);
    FLinearColor Mixed = FMath::Lerp(ColorA, ColorB, CurvedBlend);

    // Controlled mutation
    const float MutationChance = 0.15f;
    if (FMath::FRand() < MutationChance)
    {
        const float MutationIntensity = FMath::FRandRange(0.02f, 0.08f);
        const FLinearColor NeutralGray(0.5f, 0.5f, 0.5f, 1.0f);

        FLinearColor Mutation = Mixed + (FLinearColor::MakeRandomColor() - NeutralGray) * MutationIntensity;
        Mixed = FLinearColor::LerpUsingHSV(Mutation, Mixed, 0.7f).GetClamped();
    }

    return Mixed;
}

void APangeaEggActor::ApplyMaterialGroupInheritance(UMaterialInstanceDynamic* MID, const FMaterialGeneticGroup& Group) const
{
    for (const FName& Param : Group.ParameterNames)
    {
        const FLinearColor ColorA = ParentA.VisualData.Contains(Param) ? ParentA.VisualData[Param] : FLinearColor::White;
        const FLinearColor ColorB = ParentB.VisualData.Contains(Param) ? ParentB.VisualData[Param] : FLinearColor::White;

        const FLinearColor Mixed = MixParentColors(ColorA, ColorB);
        MID->SetVectorParameterValue(Param, Mixed);

        UE_LOG(LogTemp, Warning, TEXT("   %s -> (R=%.3f, G=%.3f, B=%.3f)"), *Param.ToString(), Mixed.R, Mixed.G, Mixed.B);
    }
}


void APangeaEggActor::OnRep_Species()
{
    // if (SpeciesData && ObjectMesh)
    // {
    //     // Example: blend textures based on parent traits
    //     UMaterialInstanceDynamic* MID = ObjectMesh->CreateAndSetMaterialInstanceDynamic(0);
    //     if (MID)
    //     {
    //         const float Blend = FMath::FRandRange(0.3f, 0.7f);
    //         MID->SetScalarParameterValue("TextureBlend", Blend);
    //     }
    // }
}

void APangeaEggActor::OnSaved_Implementation()
{
    Super::OnSaved_Implementation();
}

bool APangeaEggActor::ShouldBeIgnored_Implementation()
{
    return Super::ShouldBeIgnored_Implementation();
}

TArray<UActorComponent*> APangeaEggActor::GetComponentsToSave_Implementation() const
{
    return TArray<UActorComponent*>();
}





