#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Objects/UpgradeAction.h"
#include "UA_UnlockMiningSite.generated.h"

/** How an unlock action changes the unlocked state of the matching mining discovery nodes. */
UENUM(BlueprintType)
enum class EMiningUnlockMode : uint8
{
	/** Set the matching nodes to unlocked. */
	Unlock,
	/** Set the matching nodes to locked. */
	Lock,
	/** Relative mode that configuration validation rejects; migrate assets to Unlock or Lock. */
	Toggle
};

/**
 * Upgrade action that unlocks or locks every loaded mining discovery node whose facility tag matches.
 * Lets the base upgrade system gate mining sites without knowing about mining actors.
 */
UCLASS(BlueprintType, EditInlineNew)
class GF_PANGEAMININGSYSTEMRUNTIME_API UUA_UnlockMiningSite : public UUpgradeAction
{
	GENERATED_BODY()

public:
	virtual bool ValidateConfiguration_Implementation(FText& OutFailure) const override;
	virtual bool CanExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) const override;
	virtual bool TryExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) override;
	virtual bool CanReconstructOnLoad_Implementation() const override;
	virtual FGameplayTag GetUnlockedFacilityTag_Implementation() const override;

	/** Facility tag of the mining discovery nodes this action applies to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining")
	FGameplayTag FacilityTag;

	/** Whether the action unlocks or locks the matching nodes. Only Unlock and Lock pass validation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining")
	EMiningUnlockMode Mode = EMiningUnlockMode::Unlock;

	virtual void ExecuteForContext(const FBaseUpgradeContext& Context) override;
	virtual void Execute_Implementation(UObject* ContextObject) override;

private:
	/** Applies Mode to the actor when it is an authoritative discovery node with a matching facility tag. */
	void ApplyToActor(AActor* Actor) const;
};
