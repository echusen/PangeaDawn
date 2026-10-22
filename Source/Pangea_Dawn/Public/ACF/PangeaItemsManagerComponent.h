#pragma once

#include "CoreMinimal.h"
#include "ACFItemsManagerComponent.h"
#include "TimerManager.h"
#include "PangeaItemsManagerComponent.generated.h"

/** Pangea's local UI preview adapter. Use in place of the inherited vendor
 * component on a project-owned controller composition. Inventory/crafting RPCs
 * remain inherited from ACF; only preview lifecycle is owned here.
 */
UCLASS(ClassGroup = (Pangea), Blueprintable, meta = (BlueprintSpawnableComponent))
class PANGEA_DAWN_API UPangeaItemsManagerComponent : public UACFItemsManagerComponent
{
    GENERATED_BODY()

public:
    UPangeaItemsManagerComponent();

    /** Local-only asynchronous request. Legacy ACF preview getters/layout/light
     * functions continue to operate on the protected preview fields populated here.
     */
    UFUNCTION(BlueprintCallable, Category = "Preview")
    AACFCharacterPreviewStage* RequestLocalPreview();

protected:
    virtual void InitializeComponent() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient)
    TSoftClassPtr<AACFCharacterPreviewStage> ConfiguredStageClass;

    UPROPERTY(Transient)
    TWeakObjectPtr<AACFCharacter> AppearanceSource;

    FTimerHandle PreviewRetry;
    bool bCapturedConfiguration = false;
    bool bEndingPlay = false;

    void CaptureConfiguration();
    void RetryPreview();
    void OnPreviewAssetsLoaded();
    void ReleaseLocalPreview();
    bool CreateSafePreview(AACFCharacter& Player);

    UFUNCTION()
    void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

    UFUNCTION()
    void HandlePreviewDestroyed(AActor* DestroyedActor);
};
