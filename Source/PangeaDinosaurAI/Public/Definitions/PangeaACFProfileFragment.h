#pragma once

#include "CoreMinimal.h"
#include "Definitions/PangeaDefinitionFragment.h"
#include "PangeaACFProfileFragment.generated.h"

class UACFCharacterDataAsset;

/** Default ACF configuration for a creature. Explicit initializer/spawner profiles take precedence. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta=(DisplayName="ACF Character Profile"))
class PANGEADINOSAURAI_API UPangeaACFProfileFragment : public UPangeaDefinitionFragment
{
	GENERATED_BODY()

public:
	/** Hard reference deliberately keeps the default profile in the definition's cook/load dependency chain. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Profile")
	TObjectPtr<UACFCharacterDataAsset> CharacterProfile;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
