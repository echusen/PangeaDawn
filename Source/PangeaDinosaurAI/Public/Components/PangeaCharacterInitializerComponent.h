#pragma once

#include "CoreMinimal.h"
#include "Data/ACFCharacterInitializerComponent.h"
#include "PangeaCharacterInitializerComponent.generated.h"

UENUM(BlueprintType)
enum class EPangeaCharacterProfileSource : uint8
{
	Unconfigured,
	DefinitionDefault,
	ExplicitOverride
};

/** Resolves a creature definition's default before ACF initializes appearance, stats and abilities. */
UCLASS(BlueprintType, ClassGroup=(Pangea))
class PANGEADINOSAURAI_API UPangeaCharacterInitializerComponent : public UACFCharacterInitializerComponent
{
	GENERATED_BODY()

public:
	/** Read-only selection: already applied profile > explicit auto-init profile > definition default. */
	UFUNCTION(BlueprintPure, Category="Character Profile")
	UACFCharacterDataAsset* GetEffectiveCharacterProfile() const;

	UFUNCTION(BlueprintPure, Category="Character Profile")
	EPangeaCharacterProfileSource GetCharacterProfileSource() const;

	/** True after ACF's existing OnInitializationCompleted event, not merely after selecting an asset. */
	UFUNCTION(BlueprintPure, Category="Character Profile")
	bool IsCharacterProfileInitialized() const { return bProfileInitializationCompleted; }

protected:
	virtual void InitializeComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UACFCharacterDataAsset* FindDefinitionDefault() const;

	UFUNCTION()
	void HandleProfileInitializationCompleted(UACFCharacterDataAsset* Profile);

	TWeakObjectPtr<UACFCharacterDataAsset> DefinitionDefaultUsed;
	bool bProfileInitializationCompleted = false;
};
