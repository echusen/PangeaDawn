// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.
#include "Actors/PangeaEggActor.h"

#include "Components/PangeaBreedableComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Interfaces/PDDefinitionProviderInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Objects/PangeaGeneticStrategy.h"
#include "TimerManager.h"

APangeaEggActor::APangeaEggActor()
{
    bReplicates = true;
}

void APangeaEggActor::Serialize(FArchive& Ar)
{
    // Tagged legacy archives omit this field. Detect them even when loading into a live egg.
    if (Ar.IsLoading() && Ar.IsSaveGame()) EggStateVersion = 0;
    Super::Serialize(Ar);
}

void APangeaEggActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APangeaEggActor, CreatureDefinition);
    DOREPLIFETIME(APangeaEggActor, ParentA);
    DOREPLIFETIME(APangeaEggActor, ParentB);
    DOREPLIFETIME(APangeaEggActor, ChildTraits);
    DOREPLIFETIME(APangeaEggActor, EggState);
    DOREPLIFETIME(APangeaEggActor, ChildGender);
    DOREPLIFETIME(APangeaEggActor, ChildStatProfile);
    DOREPLIFETIME(APangeaEggActor, ChildAppearance);
    DOREPLIFETIME(APangeaEggActor, IncubationRemaining);
    DOREPLIFETIME(APangeaEggActor, TotalIncubationTime);
}

void APangeaEggActor::InitializeEgg(const FParentSnapshot& InA, const FParentSnapshot& InB, UPangeaGeneticStrategy* Strategy, UPangeaCreatureDefinition* InCreatureDefinition)
{
    if (!HasAuthority() || IsInitialized() || !GetWorld() || !InCreatureDefinition ||
        InA.SpeciesID.IsNone() || InA.SpeciesID != InB.SpeciesID || InA.SpeciesID != InCreatureDefinition->SpeciesId) return;
    const UPangeaBreedingFragment* Fragment = InCreatureDefinition->GetFragment<UPangeaBreedingFragment>();
    if (!Fragment || !FMath::IsFinite(Fragment->Incubation.IncubationSeconds)) return;

    ParentA = InA;
    ParentB = InB;
    CreatureDefinition = InCreatureDefinition;
    OffspringDefinition = Fragment->OffspringCreatureDefinition ? Fragment->OffspringCreatureDefinition.Get() : InCreatureDefinition;
    ChildStatsApplicationEffect = Fragment->InheritedStatsGameplayEffect;
    ChildTraits = Strategy ? Strategy->CombineTraits(ParentA, ParentB) : FGeneticTraitSet();
    ChildStatProfile = GenerateInheritedStatProfile(Fragment);
    ChildAppearance = GenerateInheritedAppearance(Fragment);
    ChildGender = OffspringDefinition->GenderAssignmentMode == ECreatureGenderAssignmentMode::Randomize
        ? (FMath::RandBool() ? ECreatureGender::Male : ECreatureGender::Female)
        : (OffspringDefinition->DefaultGender == ECreatureGender::Unspecified ? ECreatureGender::Female : OffspringDefinition->DefaultGender);
    TotalIncubationTime = FMath::Max(Fragment->Incubation.IncubationSeconds, 0.1f);
    IncubationRemaining = TotalIncubationTime;
    EggStateVersion = 1;
    EggState = EPangeaEggState::Incubating;
    StartIncubation();
    ForceNetUpdate();
}

void APangeaEggActor::ClearIncubationTimers()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HatchTimerHandle);
        World->GetTimerManager().ClearTimer(IncubationTickHandle);
    }
}

void APangeaEggActor::StartIncubation()
{
    if (!HasAuthority() || !GetWorld()) return;
    ClearIncubationTimers();
    if (EggState == EPangeaEggState::ReadyToHatch)
    {
        // Defer to a game tick, never spawn a child from inside the archive callback.
        HatchTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &APangeaEggActor::FinishIncubation);
    }
    else if (EggState == EPangeaEggState::Incubating)
    {
        if (IncubationRemaining <= 0.f)
        {
            EggState = EPangeaEggState::ReadyToHatch;
            StartIncubation();
            return;
        }
        GetWorld()->GetTimerManager().SetTimer(IncubationTickHandle, this, &APangeaEggActor::TickIncubation, 0.1f, true);
        GetWorld()->GetTimerManager().SetTimer(HatchTimerHandle, this, &APangeaEggActor::FinishIncubation, IncubationRemaining, false);
    }
    OnRep_Incubation();
}

float APangeaEggActor::GetIncubationRemaining() const
{
    if (EggState != EPangeaEggState::Incubating) return 0.f;
    if (HasAuthority() && GetWorld() && GetWorld()->GetTimerManager().TimerExists(HatchTimerHandle))
        return FMath::Max(0.f, GetWorld()->GetTimerManager().GetTimerRemaining(HatchTimerHandle));
    return FMath::IsFinite(IncubationRemaining) ? FMath::Max(0.f, IncubationRemaining) : 0.f;
}

float APangeaEggActor::GetIncubationProgress() const
{
    return TotalIncubationTime > 0.f ? FMath::Clamp(1.f - GetIncubationRemaining() / TotalIncubationTime, 0.f, 1.f) : 0.f;
}

void APangeaEggActor::TickIncubation()
{
    if (!HasAuthority()) return;
    IncubationRemaining = GetIncubationRemaining();
    OnRep_Incubation();
}

void APangeaEggActor::FinishIncubation()
{
    if (!HasAuthority() || EggState == EPangeaEggState::Hatched || EggState == EPangeaEggState::Uninitialized) return;
    ClearIncubationTimers();
    IncubationRemaining = 0.f;
    EggState = EPangeaEggState::ReadyToHatch;
    OnRep_Incubation();
    ForceNetUpdate();
    Hatch();
}

AActor* APangeaEggActor::Hatch()
{
    if (!HasAuthority() || IsActorBeingDestroyed() || bHatchInProgress || EggStateVersion != 1 ||
        (EggState != EPangeaEggState::Incubating && EggState != EPangeaEggState::ReadyToHatch) ||
        !OffspringDefinition || !GetWorld()) return nullptr;
    TGuardValue<bool> HatchGuard(bHatchInProgress, true);
    UClass* HatchClass = OffspringDefinition->CreatureClass.LoadSynchronous();
    if (!HatchClass) return nullptr;

    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    Params.bDeferConstruction = true;
    AActor* NewCreature = GetWorld()->SpawnActor<AActor>(HatchClass, GetActorLocation(), GetActorRotation(), Params);
    if (!NewCreature) return nullptr;
    IPDDefinitionProviderInterface* Provider = Cast<IPDDefinitionProviderInterface>(NewCreature);
    if (Provider && !Provider->SetCreatureDefinition_Implementation(OffspringDefinition) &&
        Provider->GetCreatureDefinition_Implementation() != OffspringDefinition)
    {
        UE_LOG(LogTemp, Error, TEXT("Hatch rejected: %s cannot accept offspring definition %s before initialization."),
            *GetNameSafe(NewCreature), *GetNameSafe(OffspringDefinition));
        NewCreature->Destroy();
        return nullptr;
    }
    // Egg presentation scale must never override the hatchling's authored scale.
    NewCreature->FinishSpawning(FTransform(GetActorRotation(), GetActorLocation()));
    if (!IsValid(NewCreature) || NewCreature->IsActorBeingDestroyed()) return nullptr;
    UPangeaBreedableComponent* Breedable = NewCreature->FindComponentByClass<UPangeaBreedableComponent>();
    if (!Breedable)
    {
        UE_LOG(LogTemp, Error, TEXT("Hatch rejected: %s has no breedable component to retain inherited state."), *GetNameSafe(NewCreature));
        NewCreature->Destroy();
        return nullptr;
    }
    Breedable->SetBirthState(ChildTraits, ChildGender, ChildAppearance, ParentA.CreatureId, ParentB.CreatureId);
    Breedable->SetInheritedStatProfile(ChildStatProfile, true, ChildStatsApplicationEffect);
    // Commit before broadcasting: a listener cannot hatch a second child by re-entering Hatch.
    ClearIncubationTimers();
    IncubationRemaining = 0.f;
    EggState = EPangeaEggState::Hatched;
    ForceNetUpdate();
    OnEggHatched.Broadcast(NewCreature);
    Destroy();
    return NewCreature;
}

FPangeaInheritedStatProfile APangeaEggActor::GenerateInheritedStatProfile(const UPangeaBreedingFragment* BreedingFragment) const
{
    FPangeaInheritedStatProfile Profile;
    if (!BreedingFragment)
    {
        return Profile;
    }

    for (const FPangeaInheritedStatRule& Rule : BreedingFragment->InheritedStatRules)
    {
        if (!Rule.StatTag.IsValid())
        {
            continue;
        }

        const float ParentAValue = GetSnapshotInheritedValue(ParentA, Rule.StatType, Rule.StatTag);
        const float ParentBValue = GetSnapshotInheritedValue(ParentB, Rule.StatType, Rule.StatTag);
        const float AverageValue = (ParentAValue + ParentBValue) * 0.5f;
        const float BestParentValue = FMath::Max(ParentAValue, ParentBValue);
        float ChildValue = FMath::Lerp(AverageValue, BestParentValue, FMath::Clamp(Rule.BestParentBias, 0.f, 1.f));

        if (FMath::FRand() < Rule.MutationChance)
        {
            ChildValue *= 1.f + FMath::FRandRange(Rule.MutationPercentMin, Rule.MutationPercentMax);
        }

        if (Rule.MaxValue > Rule.MinValue)
        {
            ChildValue = FMath::Clamp(ChildValue, Rule.MinValue, Rule.MaxValue);
        }
        else if (Rule.MinValue > 0.f)
        {
            ChildValue = FMath::Max(ChildValue, Rule.MinValue);
        }

        FPangeaInheritedStatValue& Value = Profile.Values.AddDefaulted_GetRef();
        Value.StatTag = Rule.StatTag;
        Value.StatType = Rule.StatType;
        Value.Value = ChildValue;

        UE_LOG(LogTemp, Warning, TEXT("[BreedingStats] ChildProfile Tag=%s Type=%d ParentA=%.2f ParentB=%.2f Result=%.2f"),
            *Rule.StatTag.ToString(), static_cast<int32>(Rule.StatType), ParentAValue, ParentBValue, ChildValue);
    }

    return Profile;
}

float APangeaEggActor::GetSnapshotInheritedValue(const FParentSnapshot& Snapshot, const EPangeaInheritedStatType StatType, const FGameplayTag& StatTag) const
{
    switch (StatType)
    {
    case EPangeaInheritedStatType::Statistic:
        return Snapshot.InheritedStatistics.FindRef(StatTag);

    case EPangeaInheritedStatType::PrimaryAttribute:
        return Snapshot.InheritedPrimaryAttributes.FindRef(StatTag);

    case EPangeaInheritedStatType::Attribute:
        return Snapshot.InheritedAttributes.FindRef(StatTag);

    default:
        return 0.f;
    }
}

FLinearColor APangeaEggActor::MixParentColors(const FLinearColor& ColorA, const FLinearColor& ColorB) const
{
    const UPangeaBreedingFragment* BreedingFragment = CreatureDefinition ? CreatureDefinition->GetFragment<UPangeaBreedingFragment>() : nullptr;
    const float ParentBiasPower = BreedingFragment ? BreedingFragment->ParentBiasPower : 0.8f;
    const float MutationChance = BreedingFragment ? BreedingFragment->VisualMutationChance : 0.15f;
    const float MutationIntensityMax = BreedingFragment ? BreedingFragment->VisualMutationIntensity : 0.08f;

    const float InheritBias = FMath::FRandRange(0.3f, 0.7f);
    const float CurvedBlend = FMath::Pow(InheritBias, ParentBiasPower);
    FLinearColor Mixed = FMath::Lerp(ColorA, ColorB, CurvedBlend);

    if (FMath::FRand() < MutationChance)
    {
        const float MutationIntensity = FMath::FRandRange(0.02f, MutationIntensityMax);
        const FLinearColor NeutralGray(0.5f, 0.5f, 0.5f, 1.0f);
        const FLinearColor Mutation = Mixed + (FLinearColor::MakeRandomColor() - NeutralGray) * MutationIntensity;
        Mixed = FLinearColor::LerpUsingHSV(Mutation, Mixed, 0.7f).GetClamped();
    }

    return Mixed;
}

float APangeaEggActor::MixParentScalars(float ValueA, float ValueB) const
{
    const UPangeaBreedingFragment* BreedingFragment = CreatureDefinition ? CreatureDefinition->GetFragment<UPangeaBreedingFragment>() : nullptr;
    const float ParentBiasPower = BreedingFragment ? BreedingFragment->ParentBiasPower : 0.8f;
    const float MutationChance = BreedingFragment ? BreedingFragment->VisualMutationChance : 0.15f;
    const float MutationIntensityMax = BreedingFragment ? BreedingFragment->VisualMutationIntensity : 0.08f;

    const float InheritBias = FMath::FRandRange(0.3f, 0.7f);
    const float CurvedBlend = FMath::Pow(InheritBias, ParentBiasPower);
    float Mixed = FMath::Lerp(ValueA, ValueB, CurvedBlend);

    if (FMath::FRand() < MutationChance)
    {
        Mixed += FMath::FRandRange(-MutationIntensityMax, MutationIntensityMax);
    }

    return Mixed;
}


FPangeaInheritedAppearance APangeaEggActor::GenerateInheritedAppearance(const UPangeaBreedingFragment* Fragment) const
{
    FPangeaInheritedAppearance Result;
    if (!Fragment || !Fragment->bInheritParentMaterials ||
        (ParentA.MaterialParams.IsEmpty() && ParentB.MaterialParams.IsEmpty() && ParentA.ScalarParams.IsEmpty() && ParentB.ScalarParams.IsEmpty())) return Result;
    Result.MaterialSlot = Fragment->InheritanceMaterialSlot;
    const TArray<FMaterialColorGeneticGroup>& ColorGroups = Fragment->MaterialColorGroups.IsEmpty() ? Fragment->MaterialGeneticGroups : Fragment->MaterialColorGroups;
    for (const FMaterialColorGeneticGroup& Group : ColorGroups)
        for (const FName Parameter : Group.ParameterNames)
        {
            FPangeaInheritedColor& Color = Result.Colors.AddDefaulted_GetRef();
            Color.Parameter = Parameter;
            Color.Value = MixParentColors(ParentA.MaterialParams.Contains(Parameter) ? ParentA.MaterialParams[Parameter] : FLinearColor::White,
                ParentB.MaterialParams.Contains(Parameter) ? ParentB.MaterialParams[Parameter] : FLinearColor::White);
        }
    for (const FMaterialScalarGeneticGroup& Group : Fragment->MaterialScalarGroups)
        for (const FName Parameter : Group.ParameterNames)
        {
            FPangeaInheritedScalar& Scalar = Result.Scalars.AddDefaulted_GetRef();
            Scalar.Parameter = Parameter;
            Scalar.Value = MixParentScalars(ParentA.ScalarParams.FindRef(Parameter), ParentB.ScalarParams.FindRef(Parameter));
        }
    return Result;
}

void APangeaEggActor::ApplyVisualInheritance(AActor* NewCreature)
{
    // Compatibility helper: apply the cached result, never reroll it.
    if (!IsValid(NewCreature) || ChildAppearance.IsEmpty()) return;
    USkeletalMeshComponent* Mesh = NewCreature->FindComponentByClass<USkeletalMeshComponent>();
    const int32 Slot = ChildAppearance.MaterialSlot;
    if (!Mesh || Slot < 0 || Slot >= Mesh->GetNumMaterials()) return;
    UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot));
    if (!MID) MID = Mesh->CreateAndSetMaterialInstanceDynamic(Slot);
    if (!MID) return;
    for (const FPangeaInheritedColor& Color : ChildAppearance.Colors) MID->SetVectorParameterValue(Color.Parameter, Color.Value);
    for (const FPangeaInheritedScalar& Scalar : ChildAppearance.Scalars) MID->SetScalarParameterValue(Scalar.Parameter, Scalar.Value);
}

void APangeaEggActor::OnRep_CreatureDefinition()
{
    OnRep_Incubation();
}

void APangeaEggActor::OnRep_Incubation()
{
    OnEggProgress.Broadcast(GetIncubationProgress());
}

void APangeaEggActor::OnSaved_Implementation()
{
    Super::OnSaved_Implementation();
    if (HasAuthority()) IncubationRemaining = GetIncubationRemaining();
}

void APangeaEggActor::OnLoaded_Implementation()
{
    Super::OnLoaded_Implementation();
    if (!HasAuthority()) return;
    ClearIncubationTimers();
    if (EggStateVersion != 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Egg %s has unsupported/legacy breeding state version %d; incubation not resumed. Missing historical parent data cannot be reconstructed."), *GetName(), EggStateVersion);
        return;
    }
    if (!FMath::IsFinite(TotalIncubationTime) || TotalIncubationTime <= 0.f ||
        !FMath::IsFinite(IncubationRemaining) || IncubationRemaining < 0.f || IncubationRemaining > TotalIncubationTime ||
        !CreatureDefinition || !OffspringDefinition || ChildGender == ECreatureGender::Unspecified)
    {
        UE_LOG(LogTemp, Error, TEXT("Egg %s has invalid saved incubation/outcome; incubation not resumed."), *GetName());
        return;
    }
    if (EggState == EPangeaEggState::Hatched) { Destroy(); return; }
    StartIncubation();
    ForceNetUpdate();
}

bool APangeaEggActor::ShouldBeIgnored_Implementation()
{
    return Super::ShouldBeIgnored_Implementation();
}

TArray<UActorComponent*> APangeaEggActor::GetComponentsToSave_Implementation() const
{
    return Super::GetComponentsToSave_Implementation();
}

void APangeaEggActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearIncubationTimers();
    Super::EndPlay(EndPlayReason);
}
