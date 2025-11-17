// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Objects/UpgradeAction.h"
#include "UA_SpawnNPC.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class PANGEABASEUPGRADESYSTEM_API UUA_SpawnNPC : public UUpgradeAction
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
	TSubclassOf<APawn> NPCClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
	FTransform SpawnTransform;
	
	/** GameplayTag to find the spawn marker (e.g., "Marker.Blacksmith") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
	FGameplayTag SpawnMarkerTag;

	virtual void Execute_Implementation(UObject* ContextObject) override;
};
