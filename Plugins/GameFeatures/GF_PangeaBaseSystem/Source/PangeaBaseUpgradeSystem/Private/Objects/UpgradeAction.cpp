#include "Objects/UpgradeAction.h"

bool UUpgradeAction::ValidateConfiguration_Implementation(FText& OutFailure) const
{
    OutFailure = FText::FromString(TEXT("Action must implement the checked upgrade contract; legacy void Execute cannot report success."));
    return false;
}

bool UUpgradeAction::CanExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) const
{
    OutFailure = FText::FromString(TEXT("Action has no checked preflight implementation."));
    return false;
}

bool UUpgradeAction::TryExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure)
{
    OutFailure = FText::FromString(TEXT("Action has no checked execution implementation."));
    return false;
}

bool UUpgradeAction::CanReconstructOnLoad_Implementation() const { return false; }
FGameplayTag UUpgradeAction::GetUnlockedFacilityTag_Implementation() const { return FGameplayTag(); }

void UUpgradeAction::ExecuteForContext(const FBaseUpgradeContext& Context)
{
    Execute(Context.GetPreferredActionObject());
}

void UUpgradeAction::Execute_Implementation(UObject* ContextObject) {}
