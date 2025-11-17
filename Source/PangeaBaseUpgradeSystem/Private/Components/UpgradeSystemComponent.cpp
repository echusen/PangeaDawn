// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/UpgradeSystemComponent.h"

#include "DataAssets/UpgradeLevelTable.h"
#include "DataAssets/UpgradeMilestoneData.h"
#include "Objects/UpgradeAction.h"
#include "Objects/UpgradeRequirement.h"


UUpgradeSystemComponent::UUpgradeSystemComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UUpgradeSystemComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UUpgradeSystemComponent::OnLevelIncreased(int32 NewLevel, UObject* PlayerContext)
{
    if (NewLevel <= CurrentLevel)
    {
        UE_LOG(LogTemp, Verbose, TEXT("OnLevelIncreased called with %d <= CurrentLevel %d"), NewLevel, CurrentLevel);
        return;
    }

    CurrentLevel = NewLevel;
    ExecuteMilestonesForLevel(NewLevel, PlayerContext);
}

bool UUpgradeSystemComponent::IsMilestoneCompleted(FGameplayTag MilestoneTag) const
{
    return CompletedMilestones.HasTag(MilestoneTag);
}

void UUpgradeSystemComponent::MarkMilestoneCompleted(FGameplayTag MilestoneTag)
{
    CompletedMilestones.AddTag(MilestoneTag);
}

bool UUpgradeSystemComponent::CanUpgradeToNextLevel(UObject* PlayerContext) const
{
    if (!LevelTable)
    {
        UE_LOG(LogTemp, Error, TEXT("UpgradeSystem: No LevelTable assigned!"));
        return false;
    }

    int32 TargetLevel = CurrentLevel + 1;
    TArray<UUpgradeMilestoneData*> Milestones;
    LevelTable->GetMilestonesForLevel(TargetLevel, Milestones);

    if (Milestones.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpgradeSystem: No milestones found for level %d"), TargetLevel);
        return true;
    }

    // Check if ALL milestones have their requirements met
    for (UUpgradeMilestoneData* Milestone : Milestones)
    {
        if (!Milestone)
            continue;

        for (UUpgradeRequirement* Requirement : Milestone->Requirements)
        {
            if (Requirement && !Requirement->IsRequirementMet(PlayerContext))
            {
                UE_LOG(LogTemp, Warning, TEXT("UpgradeSystem: Requirement '%s' not met for level %d"),
                    *Requirement->GetName(), TargetLevel);
                return false;
            }
        }
    }

    return true;
}

void UUpgradeSystemComponent::ExecuteMilestonesForLevel(int32 Level, UObject* PlayerContext)
{
    if (!LevelTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpgradeSystemComponent: No LevelTable assigned."));
        return;
    }

    TArray<UUpgradeMilestoneData*> Milestones;
    LevelTable->GetMilestonesForLevel(Level, Milestones);

    // ADD THIS:
    UE_LOG(LogTemp, Warning, TEXT("Found %d milestones for level %d"), Milestones.Num(), Level);

    for (UUpgradeMilestoneData* Milestone : Milestones)
    {
        if (!Milestone)
        {
            UE_LOG(LogTemp, Warning, TEXT("Milestone is null!")); // ADD THIS
            continue;
        }

        // ADD THIS:
        UE_LOG(LogTemp, Warning, TEXT("Processing milestone: %s"), *Milestone->MilestoneTag.ToString());
        UE_LOG(LogTemp, Warning, TEXT("Requirements: %d, Actions: %d"), Milestone->Requirements.Num(), Milestone->Actions.Num());

        bool bAllRequirementsMet = true;
        for (UUpgradeRequirement* Req : Milestone->Requirements)
        {
            if (!Req) continue;

            if (!Req->IsRequirementMet(PlayerContext))
            {
                bAllRequirementsMet = false;
                UE_LOG(LogTemp, Log, TEXT("Milestone %s failed requirement: %s"), *Milestone->MilestoneTag.ToString(), *GetNameSafe(Req));
                break;
            }
        }

        if (!bAllRequirementsMet)
        {
            UE_LOG(LogTemp, Warning, TEXT("Milestone %s skipped due to unmet requirements"), *Milestone->MilestoneTag.ToString()); // ADD THIS
            continue;
        }

        // ADD THIS:
        UE_LOG(LogTemp, Warning, TEXT("Executing %d actions for milestone %s"), Milestone->Actions.Num(), *Milestone->MilestoneTag.ToString());

        for (UUpgradeAction* Action : Milestone->Actions)  // Changed from ActionClasses
        {
            if (!Action)
            {
                UE_LOG(LogTemp, Warning, TEXT("Action is null"));
                continue;
            }

            UE_LOG(LogTemp, Warning, TEXT("Executing action: %s"), *GetNameSafe(Action));
            Action->Execute(GetOwner());  // No need to NewObject - already instanced
        }
        MarkMilestoneCompleted(Milestone->MilestoneTag);
    }
}

