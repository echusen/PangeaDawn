#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Objects/UpgradeAction.h"
#include "UA_UnlockMiningSite.generated.h"

UENUM(BlueprintType)
enum class EMiningUnlockMode : uint8
{
	Unlock,
	Lock,
	Toggle
};

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining")
	FGameplayTag FacilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining")
	EMiningUnlockMode Mode = EMiningUnlockMode::Unlock;

	virtual void ExecuteForContext(const FBaseUpgradeContext& Context) override;
	virtual void Execute_Implementation(UObject* ContextObject) override;

private:
	void ApplyToActor(AActor* Actor) const;
};
