// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/UpgradeSystemComponent.h"

#include "Actions/UA_EnableFacility.h"
#include "DataAssets/UpgradeMilestoneData.h"
#include "DataAssets/VillageDefinitionData.h"
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

void UUpgradeSystemComponent::LoadCompletedMilestones(UObject* PlayerContext)
{
	if (!VillageDefinition)
	{
		UE_LOG(LogTemp, Error, TEXT("[REPLAY] VillageDefinition is NULL"));
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("[REPLAY] Starting milestone replay..."));
	UE_LOG(LogTemp, Error, TEXT("[REPLAY] CompletedMilestones: %s"),
		*CompletedMilestones.ToStringSimple());

	for (const FUpgradeLevelDefinition& LevelDef : VillageDefinition->Levels)
	{
		for (const FUpgradeMilestoneDefinition& Milestone : LevelDef.Milestones)
		{
			if (!Milestone.MilestoneTag.IsValid())
				continue;

			if (!CompletedMilestones.HasTag(Milestone.MilestoneTag))
				continue;

			UE_LOG(LogTemp, Error, TEXT("[REPLAY] Replaying milestone: %s"),
				*Milestone.MilestoneTag.ToString());

			for (UUpgradeAction* Action : Milestone.Actions)
			{
				if (!Action)
					continue;

				UE_LOG(LogTemp, Error, TEXT("[REPLAY] Re-executing action: %s"),
					*GetNameSafe(Action));

				// Replay the action
				Action->Execute(GetOwner());
			}
		}
	}

	UE_LOG(LogTemp, Error, TEXT("[REPLAY] Milestone replay finished."));
}

/* -------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------- */

void UUpgradeSystemComponent::OnLevelIncreased(int32 NewLevel, UObject* PlayerContext)
{
	if (!VillageDefinition)
	{
		UE_LOG(LogTemp, Error, TEXT("UpgradeSystem: OnLevelIncreased called but VillageDefinition is null on %s"),
		       *GetOwner()->GetName());
		return;
	}

	if (NewLevel <= CurrentLevel)
	{
		UE_LOG(LogTemp, Verbose, TEXT("UpgradeSystem: Ignoring level increase to %d (current %d)"),
		       NewLevel, CurrentLevel);
		return;
	}

	CurrentLevel = NewLevel;

	UE_LOG(LogTemp, Log, TEXT("UpgradeSystem: Level increased to %d for %s"),
	       CurrentLevel, *GetOwner()->GetName());

	ExecuteMilestonesForLevel(CurrentLevel, PlayerContext);
}

bool UUpgradeSystemComponent::CanUpgradeToNextLevel(UObject* PlayerContext) const
{
	if (!VillageDefinition)
	{
		UE_LOG(LogTemp, Error, TEXT("UpgradeSystem: CanUpgradeToNextLevel called but VillageDefinition is null"));
		return false;
	}

	const int32 TargetLevel = CurrentLevel + 1;
	const FUpgradeLevelDefinition* LevelDef = FindLevelDefinition(TargetLevel);
	if (!LevelDef)
	{
		UE_LOG(LogTemp, Warning, TEXT("UpgradeSystem: No level definition found for level %d"), TargetLevel);
		// You can treat this as "nothing more to upgrade" or "blocked" – here we treat as blocked.
		return false;
	}

	// No milestones = nothing to gate, so allow upgrade.
	if (LevelDef->Milestones.Num() == 0)
	{
		return true;
	}

	for (const FUpgradeMilestoneDefinition& Milestone : LevelDef->Milestones)
	{
		for (UUpgradeRequirement* Requirement : Milestone.Requirements)
		{
			if (!Requirement)
				continue;

			if (!Requirement->IsRequirementMet(PlayerContext))
			{
				UE_LOG(LogTemp, Verbose,
					TEXT("UpgradeSystem: Requirement '%s' not met for milestone '%s' (level %d)"),
					*Requirement->GetName(),
					*Milestone.MilestoneTag.ToString(),
					TargetLevel);
				return false;
			}
		}
	}

	return true;
}

bool UUpgradeSystemComponent::IsMilestoneCompleted(FGameplayTag MilestoneTag) const
{
	return CompletedMilestones.HasTag(MilestoneTag);
}

void UUpgradeSystemComponent::MarkMilestoneCompleted(FGameplayTag MilestoneTag)
{
	if (!CompletedMilestones.HasTag(MilestoneTag))
	{
		CompletedMilestones.AddTag(MilestoneTag);

		UE_LOG(LogTemp, Error, TEXT("[UPGRADE] MarkMilestoneCompleted: %s ADDED"),
			*MilestoneTag.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UPGRADE] MarkMilestoneCompleted: %s already existed"),
			*MilestoneTag.ToString());
	}
}

/* -------------------------------------------------------------
 *  Private helpers
 * ------------------------------------------------------------- */

const FUpgradeLevelDefinition* UUpgradeSystemComponent::FindLevelDefinition(int32 Level) const
{
	if (!VillageDefinition)
		return nullptr;

	for (const FUpgradeLevelDefinition& Def : VillageDefinition->Levels)
	{
		if (Def.Level == Level)
		{
			return &Def;
		}
	}
	return nullptr;
}

void UUpgradeSystemComponent::ExecuteMilestonesForLevel(int32 Level, UObject* PlayerContext)
{
	if (!VillageDefinition)
    {
        UE_LOG(LogTemp, Error, TEXT("[UPGRADE] ExecuteMilestonesForLevel: VillageDefinition is NULL"));
        return;
    }

    UE_LOG(LogTemp, Error, TEXT("[UPGRADE] ExecuteMilestonesForLevel: Level %d"), Level);

    const FUpgradeLevelDefinition* LevelDef = FindLevelDefinition(Level);
    if (!LevelDef)
    {
        UE_LOG(LogTemp, Error, TEXT("[UPGRADE] No LevelDefinition found for level %d"), Level);
        return;
    }

    if (LevelDef->Milestones.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UPGRADE] Level %d has NO milestones"), Level);
        return;
    }

    for (const FUpgradeMilestoneDefinition& MilestoneDef : LevelDef->Milestones)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UPGRADE] Checking milestone: %s"),
            *MilestoneDef.MilestoneTag.ToString());

        // If already completed, skip
        if (MilestoneDef.MilestoneTag.IsValid() && IsMilestoneCompleted(MilestoneDef.MilestoneTag))
        {
            UE_LOG(LogTemp, Warning, TEXT("[UPGRADE] Milestone %s already completed, skipping"),
                *MilestoneDef.MilestoneTag.ToString());
            continue;
        }

        // Requirement check
        bool bAllRequirementsMet = true;

        for (UUpgradeRequirement* Requirement : MilestoneDef.Requirements)
        {
            if (!Requirement)
                continue;

            bool bMet = Requirement->IsRequirementMet(PlayerContext);

            UE_LOG(LogTemp, Warning, TEXT("[UPGRADE] Requirement %s -> %s"),
                *GetNameSafe(Requirement),
                bMet ? TEXT("MET") : TEXT("NOT MET"));

            if (!bMet)
            {
                bAllRequirementsMet = false;
                break;
            }
        }

        if (!bAllRequirementsMet)
        {
            UE_LOG(LogTemp, Warning, TEXT("[UPGRADE] Milestone %s requirements NOT met => SKIPPED"),
                *MilestoneDef.MilestoneTag.ToString());
            continue;
        }

        // Execute milestone actions
        UE_LOG(LogTemp, Error, TEXT("[UPGRADE] Executing %d actions for milestone %s"),
            MilestoneDef.Actions.Num(),
            *MilestoneDef.MilestoneTag.ToString());

        for (UUpgradeAction* Action : MilestoneDef.Actions)
        {
            if (!Action)
                continue;

            UE_LOG(LogTemp, Error, TEXT("[UPGRADE] ACTION EXECUTED: %s"),
                *GetNameSafe(Action));

            // Execute with village as the context
            Action->Execute(GetOwner());
        }

        // Mark milestone complete
        if (MilestoneDef.MilestoneTag.IsValid())
        {
            MarkMilestoneCompleted(MilestoneDef.MilestoneTag);
            UE_LOG(LogTemp, Error, TEXT("[UPGRADE] Milestone COMPLETED: %s"),
                *MilestoneDef.MilestoneTag.ToString());
        }
    }
}

//UI Helpers
bool UUpgradeSystemComponent::GetNextLevelDefinition(FUpgradeLevelDefinition& OutLevel) const
{
	if (!VillageDefinition)
		return false;

	const int32 TargetLevel = CurrentLevel + 1;

	for (const FUpgradeLevelDefinition& L : VillageDefinition->Levels)
	{
		if (L.Level == TargetLevel)
		{
			OutLevel = L;
			return true;
		}
	}
	return false;
}

void UUpgradeSystemComponent::GetMilestonesForLevel(int32 Level, TArray<FUpgradeMilestoneDefinition>& OutMilestones) const
{
	OutMilestones.Empty();
	const FUpgradeLevelDefinition* LevelDef = FindLevelDefinition(Level);
	if (LevelDef)
	{
		OutMilestones = LevelDef->Milestones;
	}
}

void UUpgradeSystemComponent::GetUnmetRequirementsForNextLevel(UObject* PlayerContext, TArray<UUpgradeRequirement*>& OutRequirements) const
{
	OutRequirements.Empty();

	if (!VillageDefinition)
		return;

	const int32 TargetLevel = CurrentLevel + 1;
	const FUpgradeLevelDefinition* LevelDef = FindLevelDefinition(TargetLevel);
	if (!LevelDef)
		return;

	for (const FUpgradeMilestoneDefinition& M : LevelDef->Milestones)
	{
		for (UUpgradeRequirement* Req : M.Requirements)
		{
			if (Req && !Req->IsRequirementMet(PlayerContext))
			{
				OutRequirements.Add(Req);
			}
		}
	}
}

void UUpgradeSystemComponent::GetFacilitiesUnlockedAtLevel(int32 Level, TArray<FGameplayTag>& OutFacilities) const
{
	OutFacilities.Empty();

	if (!VillageDefinition)
		return;

	const FUpgradeLevelDefinition* LevelDef = FindLevelDefinition(Level);
	if (!LevelDef)
		return;

	for (const FUpgradeMilestoneDefinition& M : LevelDef->Milestones)
	{
		for (UUpgradeAction* Action : M.Actions)
		{
			if (UUA_EnableFacility* Enable = Cast<UUA_EnableFacility>(Action))
			{
				if (Enable->FacilityTag.IsValid())
				{
					OutFacilities.AddUnique(Enable->FacilityTag);
				}
			}
		}
	}
}


