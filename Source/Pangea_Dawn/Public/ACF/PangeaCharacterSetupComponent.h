#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PangeaCharacterSetupComponent.generated.h"

/** Configures existing ACF components; does not add a second stats authority. */
UCLASS(ClassGroup = (Pangea), meta = (BlueprintSpawnableComponent))
class PANGEA_DAWN_API UPangeaCharacterSetupComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPangeaCharacterSetupComponent();

    UFUNCTION(BlueprintCallable, Category = "Leveling")
    bool ApplyPangeaLevelingPolicy();

protected:
    virtual void InitializeComponent() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleCharacterInitialized();
};
