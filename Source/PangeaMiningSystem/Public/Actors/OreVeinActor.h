// OreVeinActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ACFStorageComponent.h"
#include "OreVeinActor.generated.h"

class USmartObjectComponent;

UCLASS(Blueprintable)
class PANGEAMININGSYSTEM_API AOreVeinActor : public AActor
{
    GENERATED_BODY()

public:
    AOreVeinActor();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ========== COMPONENTS ==========
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UACFStorageComponent> StorageComponent;

    // ========== MINING CONFIG ==========
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining")
    float MineralsPerSecond = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining")
    int32 MaxStorageCapacity = 20;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining")
    TSubclassOf<UACFItem> CommonMaterialClass;  // Changed to AACFItem

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining")
    TSubclassOf<UACFItem> RareMaterialClass;    // Changed to AACFItem

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RareMaterialChance = 0.1f;

protected:
    UFUNCTION()
    void GenerateOre();

private:
    FTimerHandle OreGenerationTimerHandle;
};
