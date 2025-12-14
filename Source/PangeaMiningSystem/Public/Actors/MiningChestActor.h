#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ACFStorageComponent.h"
#include "MiningChestActor.generated.h"

class USmartObjectComponent;

/**
 * Generic chest that exposes a UACFStorageComponent and Smart Object slot.
 * Capacity etc. is configured by the manager or via Blueprint.
 */
UCLASS(Blueprintable)
class PANGEAMININGSYSTEM_API AMiningChestActor : public AActor
{
	GENERATED_BODY()

public:
	AMiningChestActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UACFStorageComponent> StorageComponent;
};