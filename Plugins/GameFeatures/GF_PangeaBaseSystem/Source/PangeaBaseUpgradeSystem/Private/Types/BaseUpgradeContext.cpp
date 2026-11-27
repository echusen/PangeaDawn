#include "Types/BaseUpgradeContext.h"

#include "Actors/VillageBase.h"

UObject* FBaseUpgradeContext::GetPreferredRequirementObject() const
{
	if (InteractingPawn != nullptr)
	{
		return InteractingPawn;
	}

	if (Village != nullptr)
	{
		return Village;
	}

	if (SourceObject != nullptr)
	{
		return SourceObject;
	}

	return nullptr;
}

UObject* FBaseUpgradeContext::GetPreferredActionObject() const
{
	if (Village != nullptr)
	{
		return Village;
	}

	if (InteractingPawn != nullptr)
	{
		return InteractingPawn;
	}

	if (SourceObject != nullptr)
	{
		return SourceObject;
	}

	return nullptr;
}
