// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/UpgradeSystemComponent.h"

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

	// Optional: you could re-run milestones here based on saved state
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
		UE_LOG(LogTemp, Error, TEXT("UpgradeSystem: ExecuteMilestonesForLevel called but VillageDefinition is null"));
		return;
	}

	const FUpgradeLevelDefinition* LevelDef = FindLevelDefinition(Level);
	if (!LevelDef)
	{
		UE_LOG(LogTemp, Warning, TEXT("UpgradeSystem: ExecuteMilestonesForLevel - no definition for level %d"), Level);
		return;
	}

	if (LevelDef->Milestones.Num() == 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("UpgradeSystem: No milestones defined for level %d"), Level);
		return;
	}

	for (const FUpgradeMilestoneDefinition& MilestoneDef : LevelDef->Milestones)
	{
		if (MilestoneDef.MilestoneTag.IsValid() && IsMilestoneCompleted(MilestoneDef.MilestoneTag))
		{
			UE_LOG(LogTemp, Verbose, TEXT("UpgradeSystem: Milestone %s already completed, skipping"),
			       *MilestoneDef.MilestoneTag.ToString());
			continue;
		}

		// Check requirements
		bool bAllRequirementsMet = true;
		for (UUpgradeRequirement* Requirement : MilestoneDef.Requirements)
		{
			if (!Requirement)
				continue;

			if (!Requirement->IsRequirementMet(PlayerContext))
			{
				bAllRequirementsMet = false;
				UE_LOG(LogTemp, Log,
					TEXT("UpgradeSystem: Milestone %s failed requirement: %s"),
					*MilestoneDef.MilestoneTag.ToString(),
					*GetNameSafe(Requirement));
				break;
			}
		}

		if (!bAllRequirementsMet)
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("UpgradeSystem: Milestone %s skipped due to unmet requirements"),
				*MilestoneDef.MilestoneTag.ToString());
			continue;
		}

		// Execute actions
		UE_LOG(LogTemp, Log, TEXT("UpgradeSystem: Executing %d actions for milestone %s (level %d)"),
		       MilestoneDef.Actions.Num(),
		       *MilestoneDef.MilestoneTag.ToString(),
		       Level);

		for (UUpgradeAction* Action : MilestoneDef.Actions)
		{
			if (!Action)
				continue;

			UE_LOG(LogTemp, Verbose, TEXT("UpgradeSystem: Executing action %s"), *GetNameSafe(Action));
			// We pass the village actor (owner) as context for actions.
			Action->Execute(GetOwner());
		}

		if (MilestoneDef.MilestoneTag.IsValid())
		{
			MarkMilestoneCompleted(MilestoneDef.MilestoneTag);
		}
	}
}

