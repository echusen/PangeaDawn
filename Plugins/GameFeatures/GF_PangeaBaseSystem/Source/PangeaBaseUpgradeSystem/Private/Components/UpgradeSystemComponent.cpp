#include "Components/UpgradeSystemComponent.h"
#include "Actors/VillageBase.h"
#include "Components/FacilityManagerComponent.h"
#include "Components/ACFInventoryComponent.h"
#include "DataAssets/BaseUpgradeDefinition.h"
#include "Definitions/Fragments/BaseFacilityCatalogFragment.h"
#include "Definitions/Fragments/BaseProgressionFragment.h"
#include "Definitions/Fragments/BaseSaveRulesFragment.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Objects/UpgradeAction.h"
#include "Objects/UpgradeRequirement.h"

namespace
{
FPangeaUpgradeResult MakeResult(EPangeaUpgradeResult Result, const FText& Reason = FText::GetEmpty())
{
    FPangeaUpgradeResult Value;
    Value.Result = Result;
    Value.Reason = Reason;
    return Value;
}

UACFInventoryComponent* GetPaymentInventory(const FBaseUpgradeContext& Context)
{
    return Context.InteractingPawn ? Context.InteractingPawn->FindComponentByClass<UACFInventoryComponent>() : nullptr;
}

bool CanPayCosts(const FUpgradeLevelDefinition& Level, const FBaseUpgradeContext& Context, FText& Failure)
{
    if (Level.ItemCosts.IsEmpty()) return true;
    UACFInventoryComponent* Inventory = GetPaymentInventory(Context);
    if (!Inventory)
    {
        Failure = FText::FromString(TEXT("Explicit costs require the interacting pawn's ACF inventory."));
        return false;
    }
    TMap<TSubclassOf<UACFItem>, int64> Totals;
    for (const FPangeaUpgradeItemCost& Cost : Level.ItemCosts) Totals.FindOrAdd(Cost.ItemClass) += Cost.Count;
    for (const auto& Total : Totals)
    {
        int64 Available = 0;
        for (const FInventoryItem& Item : Inventory->GetInventory())
            if (Item.ItemClass == Total.Key && !Item.bIsEquipped && Item.Count > 0) Available += Item.Count;
        if (!Total.Key || Total.Value <= 0 || Total.Value > MAX_int32 || Available < Total.Value)
        {
            Failure = FText::FromString(TEXT("Not enough unequipped items to pay the separately configured upgrade cost."));
            return false;
        }
    }
    return true;
}

bool PayCosts(const FUpgradeLevelDefinition& Level, const FBaseUpgradeContext& Context, FText& Failure)
{
    if (!CanPayCosts(Level, Context, Failure)) return false;
    if (Level.ItemCosts.IsEmpty()) return true;
    UACFInventoryComponent* Inventory = GetPaymentInventory(Context);
    if (!Inventory || !Inventory->GetOwner() || !Inventory->GetOwner()->HasAuthority()) return false;
    TMap<TSubclassOf<UACFItem>, int32> Totals;
    for (const FPangeaUpgradeItemCost& Cost : Level.ItemCosts) Totals.FindOrAdd(Cost.ItemClass) += Cost.Count;
    // Snapshot GUIDs, never mutable array indices. ACF dispatches inventory callbacks during removal.
    for (const auto& Total : Totals)
    {
        int32 Remaining = Total.Value;
        const TArray<FInventoryItem> Items = Inventory->GetInventory();
        for (const FInventoryItem& Item : Items)
        {
            if (Item.ItemClass != Total.Key || Item.bIsEquipped || Item.Count <= 0) continue;
            FInventoryItem Current;
            if (!Inventory->GetItemByGuid(Item.GetItemGuid(), Current) || Current.bIsEquipped || Current.Count <= 0) continue;
            const int32 Count = FMath::Min(Remaining, Current.Count);
            Inventory->RemoveItem(Current, Count);
            if (!IsValid(Inventory))
            {
                Failure = FText::FromString(TEXT("Payment inventory was destroyed during a callback; recovery is required."));
                return false;
            }
            FInventoryItem After;
            const int32 AfterCount = Inventory->GetItemByGuid(Current.GetItemGuid(), After) ? After.Count : 0;
            if (Current.Count - AfterCount != Count)
            {
                Failure = FText::FromString(TEXT("ACF payment delta was not confirmed; partial effects require recovery."));
                return false;
            }
            Remaining -= Count;
            if (Remaining == 0) break;
        }
        if (Remaining != 0)
        {
            Failure = FText::FromString(TEXT("Cost inventory changed during payment; partial effects require recovery."));
            return false;
        }
    }
    return true;
}
}

UPangeaUpgradeSystemComponent::UPangeaUpgradeSystemComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPangeaUpgradeSystemComponent::BeginPlay() { Super::BeginPlay(); }

void UPangeaUpgradeSystemComponent::Serialize(FArchive& Ar)
{
    if (Ar.IsLoading() && Ar.IsSaveGame())
    {
        // Missing fields in pre-B04 tagged records must not retain a live failed command's journal.
        SuccessfulActionReceipts.Reset();
        PendingOperation.Reset();
        bUpgradeNeedsRecovery = false;
    }
    Super::Serialize(Ar);
}

FBaseUpgradeContext UPangeaUpgradeSystemComponent::MakeUpgradeContext(UObject* PlayerContext) const
{
	FBaseUpgradeContext Context;
	Context.SourceObject = PlayerContext ? PlayerContext : GetOwner();
	Context.UpgradeSystem = const_cast<UPangeaUpgradeSystemComponent*>(this);
	Context.FacilityManager = GetOwner() ? GetOwner()->FindComponentByClass<UPangeaFacilityManagerComponent>() : nullptr;
	Context.Village = Cast<AVillageBase>(GetOwner());
	Context.InteractingPawn = Cast<APawn>(PlayerContext);

	if (Context.InteractingPawn)
	{
		Context.PlayerController = Cast<APlayerController>(Context.InteractingPawn->GetController());
	}
	else if (APlayerController* PlayerController = Cast<APlayerController>(PlayerContext))
	{
		Context.PlayerController = PlayerController;
		Context.InteractingPawn = PlayerController->GetPawn();
	}

	return Context;
}

FPangeaUpgradeResult UPangeaUpgradeSystemComponent::CheckNextUpgrade(UObject* PlayerContext) const
{
    if (bCommandInProgress) return MakeResult(EPangeaUpgradeResult::Busy, FText::FromString(TEXT("An upgrade command is already running.")));
    return CheckNextUpgradeInternal(PlayerContext);
}

FPangeaUpgradeResult UPangeaUpgradeSystemComponent::CheckNextUpgradeInternal(UObject* PlayerContext) const
{
    if (bUpgradeNeedsRecovery || !PendingOperation.IsEmpty())
        return MakeResult(EPangeaUpgradeResult::RecoveryRequired, FText::FromString(TEXT("An interrupted/partial upgrade requires recovery from a coherent save or an audited migration.")));
    if (!UpgradeDefinition || CurrentLevel < 0 || CurrentLevel == MAX_int32)
        return MakeResult(EPangeaUpgradeResult::InvalidDefinition, FText::FromString(TEXT("Missing definition or invalid current level.")));
    TArray<FText> Errors;
    UpgradeDefinition->GetConfigurationErrors(Errors);
    if (!Errors.IsEmpty()) return MakeResult(EPangeaUpgradeResult::InvalidDefinition, Errors[0]);
    FUpgradeLevelDefinition Level;
    if (!FindLevelDefinition(CurrentLevel + 1, Level))
        return MakeResult(EPangeaUpgradeResult::InvalidDefinition, FText::FromString(TEXT("No next authored level.")));
    const FBaseUpgradeContext Context = MakeUpgradeContext(PlayerContext);
    FText Failure;
    for (const FUpgradeMilestoneDefinition& Milestone : Level.Milestones)
    {
        if (CompletedMilestones.HasTagExact(Milestone.MilestoneTag)) continue;
        for (const UUpgradeRequirement* Requirement : Milestone.Requirements)
            if (!Requirement || !Requirement->IsRequirementMetForContext(Context))
                return MakeResult(EPangeaUpgradeResult::RequirementsNotMet,
                    Requirement ? Requirement->GetFailureMessage() : FText::FromString(TEXT("Missing requirement.")));
        for (const UUpgradeAction* Action : Milestone.Actions)
            if (!SuccessfulActionReceipts.Contains(GetActionKey(Milestone, Action)) && !Action->CanExecuteUpgrade(Context, Failure))
                return MakeResult(EPangeaUpgradeResult::ActionNotReady, Failure);
    }
    if (!CanPayCosts(Level, Context, Failure)) return MakeResult(EPangeaUpgradeResult::InsufficientItems, Failure);
    return MakeResult(EPangeaUpgradeResult::Succeeded);
}

FPangeaUpgradeResult UPangeaUpgradeSystemComponent::TryUpgradeChecked(UObject* PlayerContext)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return MakeResult(EPangeaUpgradeResult::NotAuthority);
    // A nested call must not overwrite the outer command's result.
    if (bCommandInProgress) return MakeResult(EPangeaUpgradeResult::Busy);
    TGuardValue<bool> Guard(bCommandInProgress, true);
    LastUpgradeResult = CheckNextUpgradeInternal(PlayerContext);
    if (!LastUpgradeResult.IsSuccess()) return LastUpgradeResult;
    const int32 TargetLevel = CurrentLevel + 1;
    FUpgradeLevelDefinition Level;
    if (!FindLevelDefinition(TargetLevel, Level))
    {
        LastUpgradeResult = MakeResult(EPangeaUpgradeResult::InvalidDefinition);
        return LastUpgradeResult;
    }
    const FBaseUpgradeContext Context = MakeUpgradeContext(PlayerContext);
    FText Failure;
    for (const FUpgradeMilestoneDefinition& Milestone : Level.Milestones)
    {
        if (CompletedMilestones.HasTagExact(Milestone.MilestoneTag)) continue;
        for (UUpgradeAction* Action : Milestone.Actions)
        {
            const FString Key = GetActionKey(Milestone, Action);
            if (SuccessfulActionReceipts.Contains(Key)) continue;
            PendingOperation = Key; // A save inside callbacks is detectable, not silently retryable.
            if (!Action->TryExecuteUpgrade(Context, Failure))
            {
                bUpgradeNeedsRecovery = true;
                LastUpgradeResult = MakeResult(EPangeaUpgradeResult::RecoveryRequired, Failure);
                return LastUpgradeResult;
            }
            SuccessfulActionReceipts.Add(Key);
            PendingOperation.Reset();
        }
    }
    // No payments during requirement evaluation, reconstruction or a failed preflight/action.
    // ACF has no atomic multi-item transaction: unexpected partial removal is fenced, not retried/refunded blindly.
    PendingOperation = UpgradeDefinition->GetPathName() + FString::Printf(TEXT("|Level%d|Payment"), TargetLevel);
    if (!PayCosts(Level, Context, Failure))
    {
        bUpgradeNeedsRecovery = true;
        LastUpgradeResult = MakeResult(EPangeaUpgradeResult::RecoveryRequired, Failure);
        return LastUpgradeResult;
    }
    for (const FUpgradeMilestoneDefinition& Milestone : Level.Milestones) CompletedMilestones.AddTag(Milestone.MilestoneTag);
    CurrentLevel = TargetLevel;
    PendingOperation.Reset();
    LastUpgradeResult = MakeResult(EPangeaUpgradeResult::Succeeded);
    return LastUpgradeResult;
}

FString UPangeaUpgradeSystemComponent::GetActionKey(const FUpgradeMilestoneDefinition& Milestone, const UUpgradeAction* Action) const
{
    if (!UpgradeDefinition || !Action) return FString();
    const FString Identity = Action->ActionId.IsNone() ? Action->GetPathName(UpgradeDefinition) : Action->ActionId.ToString();
    return UpgradeDefinition->GetPathName() + TEXT("|") + Milestone.MilestoneTag.ToString() + TEXT("|") + Identity;
}

bool UPangeaUpgradeSystemComponent::RestoreCompletedState(UObject* PlayerContext)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || bCommandInProgress) return false;
    if (bUpgradeNeedsRecovery || !PendingOperation.IsEmpty())
    {
        bUpgradeNeedsRecovery = true;
        LastUpgradeResult = MakeResult(EPangeaUpgradeResult::RecoveryRequired);
        return false;
    }
    if (!UpgradeDefinition) return false;
    TArray<FText> Errors;
    UpgradeDefinition->GetConfigurationErrors(Errors);
    if (!Errors.IsEmpty()) { LastUpgradeResult = MakeResult(EPangeaUpgradeResult::InvalidDefinition, Errors[0]); return false; }
    const UBaseSaveRulesFragment* Rules = UpgradeDefinition->GetFragment<UBaseSaveRulesFragment>();
    if (Rules && !Rules->bReplayCompletedMilestonesOnLoad) return true;
    TGuardValue<bool> Guard(bCommandInProgress, true);
    const FBaseUpgradeContext Context = MakeUpgradeContext(PlayerContext);
    if (Context.FacilityManager)
    {
        Context.FacilityManager->DiscoverFacilityGroups();
        for (const FFacilityEntry& Entry : Context.FacilityManager->GetAllFacilities())
            Context.FacilityManager->DisableFacility(Entry.FacilityTag);
    }
    TArray<FUpgradeLevelDefinition> Levels = GetProgressionFragment()->Levels;
    Levels.Sort([](const FUpgradeLevelDefinition& A, const FUpgradeLevelDefinition& B) { return A.Level < B.Level; });
    for (const FUpgradeLevelDefinition& Level : Levels)
    {
        if (Level.Level > CurrentLevel) break;
        for (const FUpgradeMilestoneDefinition& Milestone : Level.Milestones)
        {
            if (!CompletedMilestones.HasTagExact(Milestone.MilestoneTag)) continue;
            for (UUpgradeAction* Action : Milestone.Actions)
            {
                if (!Action->CanReconstructOnLoad()) continue;
                FText Failure;
                if (!Action->CanExecuteUpgrade(Context, Failure) || !Action->TryExecuteUpgrade(Context, Failure))
                {
                    LastUpgradeResult = MakeResult(EPangeaUpgradeResult::ActionNotReady, Failure);
                    return false;
                }
            }
        }
    }
    LastUpgradeResult = MakeResult(EPangeaUpgradeResult::Succeeded);
    return true;
}

void UPangeaUpgradeSystemComponent::LoadCompletedMilestones(UObject* PlayerContext) { RestoreCompletedState(PlayerContext); }
bool UPangeaUpgradeSystemComponent::LoadCompletedMilestonesForContext_Implementation(UObject* PlayerContext) { return RestoreCompletedState(PlayerContext); }
void UPangeaUpgradeSystemComponent::OnComponentLoaded()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    RestoreCompletedState(GetOwner());
}

void UPangeaUpgradeSystemComponent::OnLevelIncreased(int32 NewLevel, UObject* PlayerContext)
{
    if (CurrentLevel < MAX_int32 && NewLevel == CurrentLevel + 1) TryUpgradeChecked(PlayerContext);
}

bool UPangeaUpgradeSystemComponent::CanUpgradeToNextLevel(UObject* PlayerContext) const { return CheckNextUpgrade(PlayerContext).IsSuccess(); }
bool UPangeaUpgradeSystemComponent::CanUpgradeToNextLevelForContext_Implementation(UObject* PlayerContext) const { return CanUpgradeToNextLevel(PlayerContext); }
bool UPangeaUpgradeSystemComponent::IsMilestoneCompleted(FGameplayTag MilestoneTag) const { return CompletedMilestones.HasTagExact(MilestoneTag); }
void UPangeaUpgradeSystemComponent::MarkMilestoneCompleted(FGameplayTag MilestoneTag)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || bCommandInProgress || bUpgradeNeedsRecovery || !PendingOperation.IsEmpty() || !MilestoneTag.IsValid()) return;
    const UBaseProgressionFragment* Progression = GetProgressionFragment();
    if (!Progression) return;
    for (const FUpgradeLevelDefinition& Level : Progression->Levels)
        if (Level.Level <= CurrentLevel)
            for (const FUpgradeMilestoneDefinition& Milestone : Level.Milestones)
                if (Milestone.MilestoneTag == MilestoneTag) { CompletedMilestones.AddTag(MilestoneTag); return; }
}
bool UPangeaUpgradeSystemComponent::TryUpgradeToNextLevel_Implementation(UObject* PlayerContext) { return TryUpgradeChecked(PlayerContext).IsSuccess(); }

int32 UPangeaUpgradeSystemComponent::GetCurrentUpgradeLevel_Implementation() const
{
	return CurrentLevel;
}

bool UPangeaUpgradeSystemComponent::IsUpgradeMilestoneCompleted_Implementation(FGameplayTag MilestoneTag) const
{
	return IsMilestoneCompleted(MilestoneTag);
}

bool UPangeaUpgradeSystemComponent::GetNextLevelDefinition(FUpgradeLevelDefinition& OutLevel) const
{
	if (CurrentLevel == MAX_int32) return false;
	return FindLevelDefinition(CurrentLevel + 1, OutLevel);
}

bool UPangeaUpgradeSystemComponent::GetNextUpgradeLevelDefinition_Implementation(FUpgradeLevelDefinition& OutLevel) const
{
	return GetNextLevelDefinition(OutLevel);
}

void UPangeaUpgradeSystemComponent::GetMilestonesForLevel(int32 Level, TArray<FUpgradeMilestoneDefinition>& OutMilestones) const
{
	OutMilestones.Empty();

	FUpgradeLevelDefinition LevelDef;
	if (FindLevelDefinition(Level, LevelDef))
	{
		OutMilestones = MoveTemp(LevelDef.Milestones);
	}
}

void UPangeaUpgradeSystemComponent::GetUnmetRequirementsForNextLevel(UObject* PlayerContext, TArray<UUpgradeRequirement*>& OutRequirements) const
{
	OutRequirements.Empty();
	if (CurrentLevel == MAX_int32) return;

	FUpgradeLevelDefinition LevelDef;
	if (!FindLevelDefinition(CurrentLevel + 1, LevelDef))
	{
		return;
	}

	const FBaseUpgradeContext UpgradeContext = MakeUpgradeContext(PlayerContext);

	for (const FUpgradeMilestoneDefinition& Milestone : LevelDef.Milestones)
	{
		for (UUpgradeRequirement* Requirement : Milestone.Requirements)
		{
			if (Requirement && !Requirement->IsRequirementMetForContext(UpgradeContext))
			{
				OutRequirements.Add(Requirement);
			}
		}
	}
}

void UPangeaUpgradeSystemComponent::GetUnmetRequirementsForNextUpgrade_Implementation(UObject* PlayerContext, TArray<UUpgradeRequirement*>& OutRequirements) const
{
	GetUnmetRequirementsForNextLevel(PlayerContext, OutRequirements);
}

void UPangeaUpgradeSystemComponent::GetFacilitiesUnlockedAtLevel(int32 Level, TArray<FGameplayTag>& OutFacilities) const
{
	OutFacilities.Empty();

	FUpgradeLevelDefinition LevelDef;
	if (!FindLevelDefinition(Level, LevelDef))
	{
		return;
	}

	for (const FUpgradeMilestoneDefinition& Milestone : LevelDef.Milestones)
	{
		for (UUpgradeAction* Action : Milestone.Actions)
		{
			if (Action)
			{
				const FGameplayTag Tag = Action->GetUnlockedFacilityTag();
				if (Tag.IsValid()) OutFacilities.AddUnique(Tag);
			}
		}
	}
}

void UPangeaUpgradeSystemComponent::GetFacilitiesUnlockedAtUpgradeLevel_Implementation(int32 Level, TArray<FGameplayTag>& OutFacilities) const
{
	GetFacilitiesUnlockedAtLevel(Level, OutFacilities);
}

FText UPangeaUpgradeSystemComponent::GetFacilityDisplayName(const FGameplayTag& FacilityTag) const
{
	if (const UBaseFacilityCatalogFragment* Catalog = GetFacilityCatalogFragment())
	{
		for (const FFacilityGroupReference& FacilityGroup : Catalog->FacilityGroups)
		{
			if (FacilityGroup.FacilityTag == FacilityTag)
			{
				return FacilityGroup.FacilityGroupName;
			}
		}
	}

	return FText::FromString(FacilityTag.ToString());
}

FText UPangeaUpgradeSystemComponent::GetUpgradeFacilityDisplayName_Implementation(FGameplayTag FacilityTag) const
{
	return GetFacilityDisplayName(FacilityTag);
}

bool UPangeaUpgradeSystemComponent::HasDefinitionData() const
{
	return UpgradeDefinition != nullptr;
}

const UBaseProgressionFragment* UPangeaUpgradeSystemComponent::GetProgressionFragment() const
{
	return UpgradeDefinition ? UpgradeDefinition->GetFragment<UBaseProgressionFragment>() : nullptr;
}

const UBaseFacilityCatalogFragment* UPangeaUpgradeSystemComponent::GetFacilityCatalogFragment() const
{
	return UpgradeDefinition ? UpgradeDefinition->GetFragment<UBaseFacilityCatalogFragment>() : nullptr;
}

bool UPangeaUpgradeSystemComponent::ResolveLevelDefinition(int32 Level, FUpgradeLevelDefinition& OutLevel) const
{
	if (const UBaseProgressionFragment* Progression = GetProgressionFragment())
	{
		for (const FUpgradeLevelDefinition& LevelDef : Progression->Levels)
		{
			if (LevelDef.Level == Level)
			{
				OutLevel = LevelDef;
				return true;
			}
		}
	}

	return false;
}

bool UPangeaUpgradeSystemComponent::FindLevelDefinition(int32 Level, FUpgradeLevelDefinition& OutLevel) const
{
	return ResolveLevelDefinition(Level, OutLevel);
}
