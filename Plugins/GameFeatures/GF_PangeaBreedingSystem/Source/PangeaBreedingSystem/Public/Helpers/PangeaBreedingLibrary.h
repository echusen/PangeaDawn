// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PangeaBreedingLibrary.generated.h"

class AActor;
class APangeaEggActor;
class UPangeaBreedableComponent;
class UPangeaBreedingFragment;
class UPangeaCreatureDefinition;
class UPangeaGeneticStrategy;

/** Static helpers that resolve the breeding configuration (definition, fragment, egg class, genetic strategy) of a creature. */
UCLASS()
class PANGEABREEDINGSYSTEM_API UPangeaBreedingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the creature definition of an actor that implements the definition provider interface, otherwise null. */
	UFUNCTION(BlueprintPure, Category = "Pangea|Breeding")
	static UPangeaCreatureDefinition* GetCreatureDefinitionFromActor(const AActor* Actor);

	/** Returns the creature definition of the breedable's owner, or null. */
	UFUNCTION(BlueprintPure, Category = "Pangea|Breeding")
	static UPangeaCreatureDefinition* GetCreatureDefinitionFromBreedable(const UPangeaBreedableComponent* Breedable);

	/** Returns the breeding fragment of the actor's creature definition, or null. */
	UFUNCTION(BlueprintPure, Category = "Pangea|Breeding")
	static UPangeaBreedingFragment* GetBreedingFragmentFromActor(const AActor* Actor);

	/** Returns the breeding fragment of the breedable owner's creature definition, or null. */
	UFUNCTION(BlueprintPure, Category = "Pangea|Breeding")
	static UPangeaBreedingFragment* GetBreedingFragmentFromBreedable(const UPangeaBreedableComponent* Breedable);

	/** Returns the egg class configured in the breedable's breeding fragment, or FallbackEggClass when none is configured. */
	UFUNCTION(BlueprintPure, Category = "Pangea|Breeding")
	static TSubclassOf<APangeaEggActor> ResolveEggActorClass(const UPangeaBreedableComponent* Breedable, TSubclassOf<APangeaEggActor> FallbackEggClass);

	/** Creates the genetic strategy configured in the breedable's breeding fragment, or returns FallbackStrategy when none is configured. */
	UFUNCTION(BlueprintCallable, Category = "Pangea|Breeding")
	static UPangeaGeneticStrategy* CreateGeneticStrategy(UObject* Outer, const UPangeaBreedableComponent* Breedable, UPangeaGeneticStrategy* FallbackStrategy);
};
