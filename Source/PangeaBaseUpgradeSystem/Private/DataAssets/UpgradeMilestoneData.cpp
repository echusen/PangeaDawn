

#include "DataAssets/UpgradeMilestoneData.h"
#include "DataAssets/UpgradeLevelTable.h"

void UUpgradeLevelTable::GetMilestonesForLevel(int32 InLevel, TArray<UUpgradeMilestoneData*>& OutMilestones) const
{
	OutMilestones.Reset();

	for (const FUpgradeLevelEntry& Entry : LevelEntries)
	{
		if (Entry.Level == InLevel)
		{
			OutMilestones = Entry.Milestones;
			return;
		}
	}
}
