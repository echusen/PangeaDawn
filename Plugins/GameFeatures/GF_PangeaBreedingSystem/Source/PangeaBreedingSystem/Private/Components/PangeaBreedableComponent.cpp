// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Components/PangeaBreedableComponent.h"

#include "ARSFunctionLibrary.h"
#include "ARSTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actors/ACFCharacter.h"
#include "ACFGASStatisticsComponent.h"
#include "Components/PangeaBreedingFarmComponent.h"
#include "Definitions/PangeaBreedingFragment.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "Interfaces/PDDefinitionProviderInterface.h"
#include "Interfaces/PDCreatureIdentityInterface.h"
#include "Data/ACFCharacterInitializerComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

UPangeaBreedableComponent::UPangeaBreedableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UPangeaBreedableComponent::Serialize(FArchive& Ar)
{
    if (Ar.IsLoading() && Ar.IsSaveGame()) BreedingStateVersion = 0;
    Super::Serialize(Ar);
}

bool UPangeaBreedableComponent::IsFertile_Implementation() const
{
    return !bIsOnFertilityCooldown && bIsFertile;
}

bool UPangeaBreedableComponent::SetFertile_Implementation(const bool bNewFertile)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return IsFertile_Implementation();
    bIsFertile = bNewFertile;
    OnRep_Fertility();
    GetOwner()->ForceNetUpdate();
    return bIsFertile;
}

void UPangeaBreedableComponent::SetGender(const ECreatureGender NewGender)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    Gender = NewGender;
    GetOwner()->ForceNetUpdate();
}

void UPangeaBreedableComponent::BeginPlay()
{
    Super::BeginPlay();

    EnsureFallbackIdentity();
    AssignGenderFromDefinition(false);

    // Injected components can begin before ACF finishes initializing GAS attributes.
    // Reapply the saved inherited modifier after the character-data initialization.
    if (AACFCharacter* Character = Cast<AACFCharacter>(GetOwner()))
    {
        Character->OnCharacterFullyInitialized.AddUniqueDynamic(this, &UPangeaBreedableComponent::ApplyInheritedStatProfile);
        if (UACFCharacterInitializerComponent* Initializer = Character->GetInitializerComponent())
            Initializer->OnAppearanceChanged.AddUniqueDynamic(this, &UPangeaBreedableComponent::HandleAppearanceChanged);
    }

    if (!ACFAttributes && GetOwner())
    {
        ACFAttributes = GetOwner()->FindComponentByClass<UACFGASStatisticsComponent>();
    }

    if (!InheritedStatProfile.IsEmpty())
    {
        ApplyInheritedStatProfile();
    }

    if (AActor* Owner = GetOwner())
    {
        if (USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
        {
            for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
            {
                UMaterialInterface* Mat = Mesh->GetMaterial(Index);
                if (Mat && !Cast<UMaterialInstanceDynamic>(Mat))
                {
                    Mesh->CreateAndSetMaterialInstanceDynamic(Index);
                    UE_LOG(LogTemp, Warning, TEXT("%s: Converted material slot %d to dynamic."), *Owner->GetName(), Index);
                }
            }
        }
    }
    ApplyInheritedAppearance();
}

ECreatureGender UPangeaBreedableComponent::AssignGenderFromDefinition(const bool bForceReassign)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return Gender;
    if (!bForceReassign && Gender != ECreatureGender::Unspecified)
    {
        return Gender;
    }

    const UPangeaCreatureDefinition* Definition = GetCreatureDefinition();
    if (!Definition)
    {
        Gender = Gender == ECreatureGender::Unspecified ? GetRandomGender() : Gender;
        GetOwner()->ForceNetUpdate();
        return Gender;
    }

    switch (Definition->GenderAssignmentMode)
    {
    case ECreatureGenderAssignmentMode::UseDefault:
        Gender = Definition->DefaultGender == ECreatureGender::Unspecified
            ? ECreatureGender::Female
            : Definition->DefaultGender;
        break;

    case ECreatureGenderAssignmentMode::Randomize:
    default:
        Gender = GetRandomGender();
        break;
    }

    GetOwner()->ForceNetUpdate();
    return Gender;
}

void UPangeaBreedableComponent::StartFertilityCooldown(float CooldownDuration)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || !FMath::IsFinite(CooldownDuration) ||
        CooldownDuration <= 0.f || bIsOnFertilityCooldown)
    {
        return;
    }

    bIsOnFertilityCooldown = true;
    FertilityCooldownRemaining = CooldownDuration;
    ResumeFertilityCooldown();
    GetOwner()->ForceNetUpdate();

    UE_LOG(LogTemp, Warning, TEXT("%s: fertility cooldown started (%.1f s)"), *GetOwner()->GetName(), CooldownDuration);
}

void UPangeaBreedableComponent::UpdateFertilityCooldown()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()) return;
    if (!bIsOnFertilityCooldown)
    {
        GetWorld()->GetTimerManager().ClearTimer(FertilityTickTimerHandle);
        return;
    }

    FertilityCooldownRemaining = GetFertilityCooldownRemaining();
    OnFertilityCooldownTick.Broadcast(FertilityCooldownRemaining);

    if (FertilityCooldownRemaining <= 0.f)
    {
        EndFertilityCooldown();
    }
}

void UPangeaBreedableComponent::EndFertilityCooldown()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    bIsOnFertilityCooldown = false;
    FertilityCooldownRemaining = 0.0f;
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(FertilityCooldownTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(FertilityTickTimerHandle);
    }
    OnRep_Fertility();
    GetOwner()->ForceNetUpdate();

    UE_LOG(LogTemp, Warning, TEXT("%s: fertility cooldown ended"), *GetOwner()->GetName());
}

FParentSnapshot UPangeaBreedableComponent::BuildParentSnapshot() const
{
    FParentSnapshot Snapshot;
    if (const UPangeaCreatureDefinition* Definition = GetCreatureDefinition())
    {
        Snapshot.SpeciesID = Definition->SpeciesId;
    }

    Snapshot.CreatureId = GetCreatureId();
    Snapshot.Traits = GeneticTraits;
    Snapshot.ParentActor = GetOwner();
    Snapshot.VisualData = CollectMaterialGenetics();
    Snapshot.MaterialParams = Snapshot.VisualData;

    const UPangeaBreedingFragment* BreedingFragment = GetBreedingFragment();
    AActor* Owner = GetOwner();
    if (!BreedingFragment || !Owner)
    {
        return Snapshot;
    }

    // GAS inheritance does not depend on a skeletal mesh or a material slot.
    for (const FPangeaInheritedStatRule& Rule : BreedingFragment->InheritedStatRules)
    {
        const float Value = GetInheritedSourceValue(Rule.StatType, Rule.StatTag);
        switch (Rule.StatType)
        {
        case EPangeaInheritedStatType::Statistic: Snapshot.InheritedStatistics.Add(Rule.StatTag, Value); break;
        case EPangeaInheritedStatType::PrimaryAttribute: Snapshot.InheritedPrimaryAttributes.Add(Rule.StatTag, Value); break;
        case EPangeaInheritedStatType::Attribute: Snapshot.InheritedAttributes.Add(Rule.StatTag, Value); break;
        }
    }

    USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh)
    {
        return Snapshot;
    }

    const int32 SlotIndex = BreedingFragment->InheritanceMaterialSlot;
    if (SlotIndex < 0 || SlotIndex >= Mesh->GetNumMaterials())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: Invalid slot index %d for species %s"),
            *Owner->GetName(), SlotIndex, *GetNameSafe(BreedingFragment));
        return Snapshot;
    }

    UMaterialInterface* Mat = Mesh->GetMaterial(SlotIndex);
    if (!Mat)
    {
        return Snapshot;
    }

    const TArray<FMaterialColorGeneticGroup>& ColorGroups = BreedingFragment->MaterialColorGroups.Num() > 0
        ? BreedingFragment->MaterialColorGroups
        : BreedingFragment->MaterialGeneticGroups;

    for (const FMaterialColorGeneticGroup& Group : ColorGroups)
    {
        for (const FName& Param : Group.ParameterNames)
        {
            FMaterialParameterInfo Info(Param);
            FLinearColor Value = FLinearColor::White;
            Mat->GetVectorParameterValue(Info, Value);
            Snapshot.MaterialParams.Add(Param, Value);
        }
    }

    for (const FMaterialScalarGeneticGroup& Group : BreedingFragment->MaterialScalarGroups)
    {
        for (const FName& Param : Group.ParameterNames)
        {
            float Value = 0.f;
            if (Mat->GetScalarParameterValue(Param, Value))
            {
                Snapshot.ScalarParams.Add(Param, Value);
            }
        }
    }

    return Snapshot;
}

APangeaEggActor* UPangeaBreedableComponent::BreedWith(UPangeaBreedableComponent* OtherParent, UPangeaBreedingFarmComponent* Farm)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(OtherParent) || !IsValid(Farm) || OtherParent == this ||
        !IPDBreedableInterface::Execute_IsFertile(this) || !IPDBreedableInterface::Execute_IsFertile(OtherParent))
    {
        return nullptr;
    }

    const FParentSnapshot MySnapshot = BuildParentSnapshot();
    const FParentSnapshot OtherSnapshot = OtherParent->BuildParentSnapshot();
    if (MySnapshot.SpeciesID != OtherSnapshot.SpeciesID)
    {
        return nullptr;
    }

    APangeaEggActor* Egg = Gender == ECreatureGender::Male
        ? Farm->TryBreed(this, OtherParent) : Farm->TryBreed(OtherParent, this);
    if (Egg)
    {
        OnBred.Broadcast(Egg, OtherParent->GetOwner());
    }

    return Egg;
}

void UPangeaBreedableComponent::PullAttributesIntoTraits_Implementation(FGeneticTraitSet& InOutTraits) const
{
}

void UPangeaBreedableComponent::SetInheritedStatProfile(const FPangeaInheritedStatProfile& NewProfile, const bool bApplyImmediately,
    TSubclassOf<UGameplayEffect> ApplicationEffect)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    InheritedStatProfile = NewProfile;
    InheritedStatsApplicationEffect = ApplicationEffect;
    GetOwner()->ForceNetUpdate();

    if (bApplyImmediately)
    {
        ApplyInheritedStatProfile();
    }
}

void UPangeaBreedableComponent::ApplyInheritedStatProfile()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
    if (InheritedStatsEffectHandle.IsValid() && ASC && InheritedStatProfile.IsEmpty())
    {
        ASC->RemoveActiveGameplayEffect(InheritedStatsEffectHandle);
        InheritedStatsEffectHandle.Invalidate();
    }
    if (InheritedStatProfile.IsEmpty())
    {
        return;
    }

    if (!ACFAttributes && GetOwner())
    {
        ACFAttributes = GetOwner()->FindComponentByClass<UACFGASStatisticsComponent>();
    }

    const UPangeaBreedingFragment* BreedingFragment = GetBreedingFragment();
    TSubclassOf<UGameplayEffect> Effect = InheritedStatsApplicationEffect;
    if (!Effect && BreedingFragment) Effect = BreedingFragment->InheritedStatsGameplayEffect;
    if (!ASC || !Effect)
    {
        return;
    }

    if (Effect.GetDefaultObject()->DurationPolicy != EGameplayEffectDurationType::Infinite)
    {
        UE_LOG(LogTemp, Error, TEXT("Inherited stats on %s require an Infinite GameplayEffect; instant effects cannot be safely reapplied after load."), *GetNameSafe(GetOwner()));
        return;
    }

    if (InheritedStatsEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(InheritedStatsEffectHandle);
        InheritedStatsEffectHandle.Invalidate();
    }

    FAttributesSetModifier Modifier;
    Modifier.GEModifierType = EGEType::ESetByCallerFromConfig;
    Modifier.GameplayEffectModifier = Effect;

    for (const FPangeaInheritedStatValue& Value : InheritedStatProfile.Values)
    {
        switch (Value.StatType)
        {
        case EPangeaInheritedStatType::Statistic:
            Modifier.StatisticsMod.Add(FStatisticsModifier(Value.StatTag, EModifierType::EAdditive, Value.Value, 0.f));
            break;

        case EPangeaInheritedStatType::PrimaryAttribute:
            Modifier.PrimaryAttributesMod.Add(FAttributeModifier(Value.StatTag, EModifierType::EAdditive, Value.Value));
            break;

        case EPangeaInheritedStatType::Attribute:
            Modifier.AttributesMod.Add(FAttributeModifier(Value.StatTag, EModifierType::EAdditive, Value.Value));
            break;
        }
    }

    if (Modifier.PrimaryAttributesMod.Num() == 0 && Modifier.AttributesMod.Num() == 0 && Modifier.StatisticsMod.Num() == 0)
    {
        return;
    }

    InheritedStatsEffectHandle = UARSFunctionLibrary::CreateAndApplyGameplayEffectFromAttributeModifier(Modifier, ASC);
    UE_LOG(LogTemp, Warning, TEXT("[BreedingStats] Applied inherited profile to %s Primary=%d Secondary=%d Statistics=%d Effect=%s"),
        *GetNameSafe(GetOwner()),
        Modifier.PrimaryAttributesMod.Num(),
        Modifier.AttributesMod.Num(),
        Modifier.StatisticsMod.Num(),
        *GetNameSafe(Effect.Get()));
}

void UPangeaBreedableComponent::OnComponentLoaded()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(FertilityCooldownTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(FertilityTickTimerHandle);
    }
    if (BreedingStateVersion > 1)
    {
        UE_LOG(LogTemp, Error, TEXT("Unsupported breeding save version %d on %s; state was not reinitialized."), BreedingStateVersion, *GetNameSafe(GetOwner()));
        return;
    }
    if (BreedingStateVersion == 0)
        UE_LOG(LogTemp, Warning, TEXT("Legacy breeding record on %s: unsaved historical gender/cooldown/identity cannot be recovered; retaining available defaults."), *GetNameSafe(GetOwner()));
    BreedingStateVersion = 1;
    EnsureFallbackIdentity();
    AssignGenderFromDefinition(false);
    ResumeFertilityCooldown();
    // ACF normalizes current resources when maxima change. On load those currents
    // are saved absolute values, not ratios against the fresh actor's baseline.
    UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
    TArray<TPair<FGameplayAttribute, float>> SavedResources;
    if (ASC && GetOwner() && GetOwner()->HasAuthority())
        for (const FPangeaInheritedStatValue& Value : InheritedStatProfile.Values)
        {
            FStatisticsConfig Config;
            if (Value.StatType == EPangeaInheritedStatType::Statistic &&
                UARSFunctionLibrary::TryGetStatisticFromSetByCallerTag(Value.StatTag, Config) &&
                ASC->HasAttributeSetForAttribute(Config.CurrentStatAttribute))
                SavedResources.Emplace(Config.CurrentStatAttribute, ASC->GetNumericAttribute(Config.CurrentStatAttribute));
        }
    ApplyInheritedStatProfile();
    for (const TPair<FGameplayAttribute, float>& Resource : SavedResources)
        ASC->SetNumericAttributeBase(Resource.Key, Resource.Value); // AttributeSet performs its normal clamping.
    ApplyInheritedAppearance();
    GetOwner()->ForceNetUpdate();
}

void UPangeaBreedableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AACFCharacter* Character = Cast<AACFCharacter>(GetOwner()))
    {
        Character->OnCharacterFullyInitialized.RemoveDynamic(this, &UPangeaBreedableComponent::ApplyInheritedStatProfile);
        if (UACFCharacterInitializerComponent* Initializer = Character->GetInitializerComponent())
            Initializer->OnAppearanceChanged.RemoveDynamic(this, &UPangeaBreedableComponent::HandleAppearanceChanged);
    }
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(FertilityCooldownTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(FertilityTickTimerHandle);
    }
    if (InheritedStatsEffectHandle.IsValid())
        if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent()) ASC->RemoveActiveGameplayEffect(InheritedStatsEffectHandle);
    InheritedStatsEffectHandle.Invalidate();
    Super::EndPlay(EndPlayReason);
}

void UPangeaBreedableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UPangeaBreedableComponent, Gender);
    DOREPLIFETIME(UPangeaBreedableComponent, bIsFertile);
    DOREPLIFETIME(UPangeaBreedableComponent, bIsOnFertilityCooldown);
    DOREPLIFETIME(UPangeaBreedableComponent, FertilityCooldownRemaining);
    DOREPLIFETIME(UPangeaBreedableComponent, GeneticTraits);
    DOREPLIFETIME(UPangeaBreedableComponent, InheritedStatProfile);
    DOREPLIFETIME(UPangeaBreedableComponent, InheritedAppearance);
    DOREPLIFETIME(UPangeaBreedableComponent, ParentAId);
    DOREPLIFETIME(UPangeaBreedableComponent, ParentBId);
    DOREPLIFETIME(UPangeaBreedableComponent, FallbackCreatureId);
}

FGuid UPangeaBreedableComponent::GetCreatureId() const
{
    if (const IPDCreatureIdentityInterface* Identity = Cast<IPDCreatureIdentityInterface>(GetOwner()))
        return Identity->GetCreatureId_Implementation();
    return FallbackCreatureId;
}

void UPangeaBreedableComponent::EnsureFallbackIdentity()
{
    if (GetOwner() && GetOwner()->HasAuthority() && !Cast<IPDCreatureIdentityInterface>(GetOwner()) && !FallbackCreatureId.IsValid())
        FallbackCreatureId = FGuid::NewGuid();
}

float UPangeaBreedableComponent::GetFertilityCooldownRemaining() const
{
    if (GetOwner() && GetOwner()->HasAuthority() && GetWorld() && GetWorld()->GetTimerManager().TimerExists(FertilityCooldownTimerHandle))
        return FMath::Max(0.f, GetWorld()->GetTimerManager().GetTimerRemaining(FertilityCooldownTimerHandle));
    return bIsOnFertilityCooldown && FMath::IsFinite(FertilityCooldownRemaining) ? FMath::Max(0.f, FertilityCooldownRemaining) : 0.f;
}

void UPangeaBreedableComponent::ResumeFertilityCooldown()
{
    if (!GetWorld() || !GetOwner() || !GetOwner()->HasAuthority()) return;
    FTimerManager& Timers = GetWorld()->GetTimerManager();
    Timers.ClearTimer(FertilityCooldownTimerHandle);
    Timers.ClearTimer(FertilityTickTimerHandle);
    if (bIsOnFertilityCooldown && FMath::IsFinite(FertilityCooldownRemaining) && FertilityCooldownRemaining > 0.f)
    {
        Timers.SetTimer(FertilityCooldownTimerHandle, this, &UPangeaBreedableComponent::EndFertilityCooldown, FertilityCooldownRemaining, false);
        Timers.SetTimer(FertilityTickTimerHandle, this, &UPangeaBreedableComponent::UpdateFertilityCooldown, 1.f, true);
    }
    else { bIsOnFertilityCooldown = false; FertilityCooldownRemaining = 0.f; }
    OnRep_Fertility();
}

void UPangeaBreedableComponent::OnComponentSaved()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || BreedingStateVersion > 1) return;
    EnsureFallbackIdentity();
    FertilityCooldownRemaining = GetFertilityCooldownRemaining();
    BreedingStateVersion = 1;
}

void UPangeaBreedableComponent::OnRep_Fertility()
{
    OnFertilityStateChanged.Broadcast(IsFertile_Implementation());
    OnFertilityCooldownTick.Broadcast(FertilityCooldownRemaining);
}

void UPangeaBreedableComponent::SetBirthState(const FGeneticTraitSet& Traits, ECreatureGender InGender,
    const FPangeaInheritedAppearance& Appearance, FGuid InParentAId, FGuid InParentBId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    GeneticTraits = Traits;
    Gender = InGender;
    InheritedAppearance = Appearance;
    ParentAId = InParentAId;
    ParentBId = InParentBId;
    ApplyInheritedAppearance();
    GetOwner()->ForceNetUpdate();
}

void UPangeaBreedableComponent::OnRep_InheritedAppearance() { ApplyInheritedAppearance(); }
void UPangeaBreedableComponent::HandleAppearanceChanged(UACFCharacterDataAsset* Profile) { ApplyInheritedAppearance(); }

void UPangeaBreedableComponent::ApplyInheritedAppearance()
{
    if (InheritedAppearance.IsEmpty() || !GetOwner()) return;
    USkeletalMeshComponent* Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh || InheritedAppearance.MaterialSlot < 0 || InheritedAppearance.MaterialSlot >= Mesh->GetNumMaterials()) return;
    UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(InheritedAppearance.MaterialSlot));
    if (!MID) MID = Mesh->CreateAndSetMaterialInstanceDynamic(InheritedAppearance.MaterialSlot);
    if (!MID) return;
    for (const FPangeaInheritedColor& Color : InheritedAppearance.Colors) MID->SetVectorParameterValue(Color.Parameter, Color.Value);
    for (const FPangeaInheritedScalar& Scalar : InheritedAppearance.Scalars) MID->SetScalarParameterValue(Scalar.Parameter, Scalar.Value);
}

TMap<FName, FLinearColor> UPangeaBreedableComponent::CollectMaterialGenetics() const
{
    TMap<FName, FLinearColor> Out;

    const UPangeaBreedingFragment* BreedingFragment = GetBreedingFragment();
    if (!BreedingFragment || !BreedingFragment->bInheritParentMaterials)
    {
        return Out;
    }

    USkeletalMeshComponent* Mesh = GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
    if (!Mesh)
    {
        return Out;
    }

    const int32 SlotIndex = BreedingFragment->InheritanceMaterialSlot;
    if (SlotIndex < 0 || SlotIndex >= Mesh->GetNumMaterials())
    {
        return Out;
    }

    UMaterialInterface* Mat = Mesh->GetMaterial(SlotIndex);
    if (!Mat)
    {
        return Out;
    }

    const TArray<FMaterialColorGeneticGroup>& ColorGroups = BreedingFragment->MaterialColorGroups.Num() > 0
        ? BreedingFragment->MaterialColorGroups
        : BreedingFragment->MaterialGeneticGroups;

    for (const FMaterialColorGeneticGroup& Group : ColorGroups)
    {
        for (const FName& Param : Group.ParameterNames)
        {
            FLinearColor Value;
            if (Mat->GetVectorParameterValue(Param, Value))
            {
                Out.Add(Param, Value);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("CollectMaterialGenetics: %d params from %s (slot %d)"),
        Out.Num(), *GetOwner()->GetName(), SlotIndex);

    return Out;
}

UPangeaCreatureDefinition* UPangeaBreedableComponent::GetCreatureDefinition() const
{
    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->GetClass()->ImplementsInterface(UPDDefinitionProviderInterface::StaticClass()))
    {
        return nullptr;
    }

    return IPDDefinitionProviderInterface::Execute_GetCreatureDefinition(Owner);
}

const UPangeaBreedingFragment* UPangeaBreedableComponent::GetBreedingFragment() const
{
    UPangeaCreatureDefinition* Definition = GetCreatureDefinition();
    return Definition ? Definition->GetFragment<UPangeaBreedingFragment>() : nullptr;
}

float UPangeaBreedableComponent::GetInheritedSourceValue(const EPangeaInheritedStatType StatType, const FGameplayTag& StatTag) const
{
    UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
    if (!ASC || !StatTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BreedingStats] Missing ASC or invalid tag on %s for tag=%s"),
            *GetNameSafe(GetOwner()), *StatTag.ToString());
        return 0.f;
    }

    switch (StatType)
    {
    case EPangeaInheritedStatType::Statistic:
    {
        FStatisticsConfig StatConfig;
        if (!UARSFunctionLibrary::TryGetStatisticFromSetByCallerTag(StatTag, StatConfig))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BreedingStats] Failed to resolve statistic tag %s"), *StatTag.ToString());
            return 0.f;
        }

        bool bFound = false;
        const float Value = ASC->GetGameplayAttributeValue(StatConfig.MaxStatAttribute, bFound);
        UE_LOG(LogTemp, Warning, TEXT("[BreedingStats] Parent=%s Type=Statistic Tag=%s Value=%.2f"),
            *GetNameSafe(GetOwner()), *StatTag.ToString(), Value);
        return Value;
    }

    case EPangeaInheritedStatType::PrimaryAttribute:
    case EPangeaInheritedStatType::Attribute:
    {
        FGameplayAttribute Attribute;
        if (!UARSFunctionLibrary::TryGetAttributeFromSetByCallerTag(StatTag, Attribute))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BreedingStats] Failed to resolve attribute tag %s"), *StatTag.ToString());
            return 0.f;
        }

        bool bFound = false;
        const float Value = ASC->GetGameplayAttributeValue(Attribute, bFound);
        UE_LOG(LogTemp, Warning, TEXT("[BreedingStats] Parent=%s Type=%s Tag=%s Value=%.2f"),
            *GetNameSafe(GetOwner()),
            StatType == EPangeaInheritedStatType::PrimaryAttribute ? TEXT("PrimaryAttribute") : TEXT("Attribute"),
            *StatTag.ToString(),
            Value);
        return Value;
    }

    default:
        return 0.f;
    }
}

UAbilitySystemComponent* UPangeaBreedableComponent::GetOwnerAbilitySystemComponent() const
{
    return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
}

ECreatureGender UPangeaBreedableComponent::GetRandomGender()
{
    return FMath::RandBool() ? ECreatureGender::Male : ECreatureGender::Female;
}
