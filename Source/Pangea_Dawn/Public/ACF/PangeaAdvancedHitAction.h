#pragma once

#include "CoreMinimal.h"
#include "Actions/ACFActionAbility.h"
#include "Game/ACFDamageType.h"
#include "PangeaAdvancedHitAction.generated.h"

/** Pangea hit reaction using ACF's supported action base. The vendor advanced
 * action has a private constructor, so native inheritance from it is not supported.
 */
UCLASS()
class PANGEA_DAWN_API UPangeaAdvancedHitAction : public UACFActionAbility
{
    GENERATED_BODY()

public:
    UPangeaAdvancedHitAction();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hit Reaction")
    TMap<EACFDirection, FName> HitDirectionToMontageSectionMap;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hit Reaction")
    TArray<FBoneSections> FrontDetailsSectionByBoneNames;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Warp")
    float HitWarpDistance = 200.f;

    virtual FTransform GetWarpTransform_Implementation() override;
    virtual FName GetMontageSectionName_Implementation() override;
    virtual void OnActionStarted_Implementation() override;
};
