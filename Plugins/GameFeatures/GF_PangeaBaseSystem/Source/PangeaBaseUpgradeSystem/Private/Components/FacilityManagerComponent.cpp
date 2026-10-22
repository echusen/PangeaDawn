#include "Components/FacilityManagerComponent.h"

#include "Actors/FacilityGroup.h"
#include "GameFramework/Actor.h"

UPangeaFacilityManagerComponent::UPangeaFacilityManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPangeaFacilityManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    DiscoverFacilityGroups();
}

void UPangeaFacilityManagerComponent::DiscoverFacilityGroups()
{
    Facilities.Empty();

    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    TArray<AActor*> AttachedActors;
    Owner->GetAttachedActors(AttachedActors);

    for (AActor* Child : AttachedActors)
    {
        if (AFacilityGroup* Group = Cast<AFacilityGroup>(Child))
        {
            FFacilityEntry Entry;
            Entry.FacilityTag = Group->GetFacilityTag();
            Entry.Group = Group;
            Entry.bUnlocked = false;

            Facilities.Add(Entry);
        }
    }
}

bool UPangeaFacilityManagerComponent::IsFacilityEnabled(const FGameplayTag& FacilityTag) const
{
    for (const auto& Entry : Facilities)
    {
        if (Entry.FacilityTag == FacilityTag)
            return Entry.bUnlocked;
    }
    return false;
}

void UPangeaFacilityManagerComponent::EnableFacility(const FGameplayTag& FacilityTag)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    for (auto& Entry : Facilities)
    {
        if (Entry.FacilityTag == FacilityTag)
        {
            Entry.bUnlocked = true;

            if (Entry.Group.IsValid())
            {
                Entry.Group->SetFacilityEnabled(true);
            }
            return;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("FacilityManager: Facility '%s' not found"), *FacilityTag.ToString());
}

void UPangeaFacilityManagerComponent::DisableFacility(const FGameplayTag& FacilityTag)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    for (auto& Entry : Facilities)
    {
        if (Entry.FacilityTag == FacilityTag)
        {
            Entry.bUnlocked = false;

            if (Entry.Group.IsValid())
            {
                Entry.Group->SetFacilityEnabled(false);
            }
            return;
        }
    }
}
