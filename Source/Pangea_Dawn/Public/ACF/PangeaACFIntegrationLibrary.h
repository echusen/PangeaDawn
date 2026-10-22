#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PangeaACFIntegrationLibrary.generated.h"

class AActor;
class UARSLevelingComponent;

/** Project-owned adapters. Vendor ACF code and assets remain unmodified. */
UCLASS()
class PANGEA_DAWN_API UPangeaACFIntegrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Unit vector away from Target; zero for missing or coincident actors. */
    UFUNCTION(BlueprintPure, Category = "Combat")
    static FVector GetSafeRelativeDirection(const AActor* Source, const AActor* Target);

    /** Supply a transient constant death-reward curve only when no authored curve exists.
     * Preserves fixed reward, leveling mode and any authored level-based override.
     * Call again after replacing character initialization data if it clears the override.
     */
    UFUNCTION(BlueprintCallable, Category = "Leveling")
    static bool EnsureFixedDeathRewardCurve(UARSLevelingComponent* Leveling);
};
