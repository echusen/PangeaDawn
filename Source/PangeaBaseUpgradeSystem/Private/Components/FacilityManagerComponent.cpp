#include "Components/FacilityManagerComponent.h"

#include "Actors/FacilityGroup.h"
#include "GameFramework/Actor.h"

UFacilityManagerComponent::UFacilityManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UFacilityManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    DiscoverFacilityGroups();
}

void UFacilityManagerComponent::DiscoverFacilityGroups()
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

bool UFacilityManagerComponent::IsFacilityEnabled(const FGameplayTag& FacilityTag) const
{
    for (const auto& Entry : Facilities)
    {
        if (Entry.FacilityTag == FacilityTag)
            return Entry.bUnlocked;
    }
    return false;
}

void UFacilityManagerComponent::EnableFacility(const FGameplayTag& FacilityTag)
{
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

void UFacilityManagerComponent::DisableFacility(const FGameplayTag& FacilityTag)
{
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
