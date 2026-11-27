#include "Actions/UA_UnlockMiningSite.h"

#include "Engine/World.h"
#include "EngineUtils.h"

#include "Actors/MiningDiscoveryNodeActor.h"
#include "Actors/VillageBase.h"
#include "Types/BaseUpgradeContext.h"

namespace UA_UnlockMiningSiteHelpers
{
UWorld* GetUpgradeWorld(const FBaseUpgradeContext& Context)
{
	if (Context.Village != nullptr)
	{
		return Context.Village->GetWorld();
	}

	UObject* const Object = Context.GetPreferredActionObject();
	return Object != nullptr ? Object->GetWorld() : nullptr;
}
} // namespace UA_UnlockMiningSiteHelpers

bool UUA_UnlockMiningSite::ValidateConfiguration_Implementation(FText& OutFailure) const
{
	if (!FacilityTag.IsValid() || (Mode != EMiningUnlockMode::Unlock && Mode != EMiningUnlockMode::Lock))
	{
		OutFailure = FText::FromString(TEXT("Mining action needs a valid facility tag and an absolute Unlock/Lock mode; migrate Toggle explicitly."));
		return false;
	}

	return true;
}

bool UUA_UnlockMiningSite::CanExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) const
{
	if (!ValidateConfiguration(OutFailure))
	{
		return false;
	}

	if (UWorld* World = UA_UnlockMiningSiteHelpers::GetUpgradeWorld(Context))
	{
		for (TActorIterator<AMiningDiscoveryNodeActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->FacilityTag == FacilityTag)
			{
				return true;
			}
		}
	}

	OutFailure = FText::FromString(TEXT("No matching mining discovery node is currently loaded."));
	return false;
}

bool UUA_UnlockMiningSite::TryExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure)
{
	UWorld* const World = UA_UnlockMiningSiteHelpers::GetUpgradeWorld(Context);
	if (World == nullptr || World->GetNetMode() == NM_Client || !CanExecuteUpgrade(Context, OutFailure))
	{
		return false;
	}

	bool bFound = false;

	for (TActorIterator<AMiningDiscoveryNodeActor> It(World); It; ++It)
	{
		if (!IsValid(*It) || It->FacilityTag != FacilityTag)
		{
			continue;
		}

		if (!It->HasAuthority())
		{
			return false;
		}

		bFound = true;
		ApplyToActor(*It);

		if (!IsValid(*It) || It->bUnlocked != (Mode == EMiningUnlockMode::Unlock))
		{
			OutFailure = FText::FromString(TEXT("Mining node did not confirm its requested unlock state."));
			return false;
		}
	}

	return bFound;
}

bool UUA_UnlockMiningSite::CanReconstructOnLoad_Implementation() const
{
	return Mode == EMiningUnlockMode::Unlock || Mode == EMiningUnlockMode::Lock;
}

FGameplayTag UUA_UnlockMiningSite::GetUnlockedFacilityTag_Implementation() const
{
	return Mode == EMiningUnlockMode::Unlock ? FacilityTag : FGameplayTag();
}

void UUA_UnlockMiningSite::ExecuteForContext(const FBaseUpgradeContext& Context)
{
	FText Failure;
	TryExecuteUpgrade(Context, Failure);
}

void UUA_UnlockMiningSite::Execute_Implementation(UObject* ContextObject)
{
	FBaseUpgradeContext Context;
	Context.SourceObject = ContextObject;
	Context.Village = Cast<AVillageBase>(ContextObject);
	ExecuteForContext(Context);
}

void UUA_UnlockMiningSite::ApplyToActor(AActor* Actor) const
{
	AMiningDiscoveryNodeActor* const Node = Cast<AMiningDiscoveryNodeActor>(Actor);
	if (Node == nullptr || !Node->HasAuthority() || !FacilityTag.IsValid() || Node->FacilityTag != FacilityTag)
	{
		return;
	}

	if (Mode == EMiningUnlockMode::Unlock)
	{
		Node->SetUnlocked(true);
	}
	else if (Mode == EMiningUnlockMode::Lock)
	{
		Node->SetUnlocked(false);
	}
}
