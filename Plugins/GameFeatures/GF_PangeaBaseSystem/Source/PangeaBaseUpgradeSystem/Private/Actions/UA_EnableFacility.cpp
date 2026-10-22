#include "Actions/UA_EnableFacility.h"
#include "Actors/VillageBase.h"
#include "Components/FacilityManagerComponent.h"
#include "GameFramework/Actor.h"

bool UUA_EnableFacility::ValidateConfiguration_Implementation(FText& OutFailure) const
{
    if (!FacilityTag.IsValid() || (Mode != EFacilityEnableMode::EnableOnly && Mode != EFacilityEnableMode::DisableOnly))
    {
        OutFailure = FText::FromString(TEXT("Facility action needs a valid tag and an absolute EnableOnly/DisableOnly mode; migrate Toggle explicitly."));
        return false;
    }
    return true;
}

bool UUA_EnableFacility::CanExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) const
{
    if (!ValidateConfiguration(OutFailure)) return false;
    int32 Matches = 0;
    bool bLoaded = false;
    if (Context.FacilityManager)
        for (const FFacilityEntry& Entry : Context.FacilityManager->GetAllFacilities())
            if (Entry.FacilityTag == FacilityTag) { ++Matches; bLoaded = Entry.Group.IsValid(); }
    if (Matches == 1 && bLoaded) return true;
    OutFailure = FText::FromString(FString::Printf(TEXT("Facility %s must resolve to exactly one loaded group on this base."), *FacilityTag.ToString()));
    return false;
}

bool UUA_EnableFacility::TryExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure)
{
    if (!Context.FacilityManager || !Context.FacilityManager->GetOwner() ||
        !Context.FacilityManager->GetOwner()->HasAuthority() || !CanExecuteUpgrade(Context, OutFailure)) return false;
    const bool bEnable = Mode == EFacilityEnableMode::EnableOnly;
    if (bEnable) Context.FacilityManager->EnableFacility(FacilityTag);
    else Context.FacilityManager->DisableFacility(FacilityTag);
    return CanExecuteUpgrade(Context, OutFailure) && Context.FacilityManager->IsFacilityEnabled(FacilityTag) == bEnable;
}

bool UUA_EnableFacility::CanReconstructOnLoad_Implementation() const
{
    return Mode == EFacilityEnableMode::EnableOnly || Mode == EFacilityEnableMode::DisableOnly;
}

FGameplayTag UUA_EnableFacility::GetUnlockedFacilityTag_Implementation() const
{
    return Mode == EFacilityEnableMode::EnableOnly ? FacilityTag : FGameplayTag();
}

void UUA_EnableFacility::ExecuteForContext(const FBaseUpgradeContext& Context)
{
    FText Failure;
    TryExecuteUpgrade(Context, Failure);
}

void UUA_EnableFacility::Execute_Implementation(UObject* ContextObject)
{
    AActor* Actor = Cast<AActor>(ContextObject);
    if (!Actor || !Actor->HasAuthority()) return;
    FBaseUpgradeContext Context;
    Context.SourceObject = Actor;
    Context.FacilityManager = Actor->FindComponentByClass<UPangeaFacilityManagerComponent>();
    ExecuteForContext(Context);
}
