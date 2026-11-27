// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Helpers/PangeaBreedingLibrary.h"

#include "Actors/PangeaEggActor.h"
#include "Components/PangeaBreedableComponent.h"
#include "Definitions/PangeaBreedingFragment.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "Interfaces/PDDefinitionProviderInterface.h"
#include "Objects/PangeaGeneticStrategy.h"

UPangeaCreatureDefinition* UPangeaBreedingLibrary::GetCreatureDefinitionFromActor(const AActor* Actor)
{
	if (Actor == nullptr || !Actor->GetClass()->ImplementsInterface(UPDDefinitionProviderInterface::StaticClass()))
	{
		return nullptr;
	}

	return IPDDefinitionProviderInterface::Execute_GetCreatureDefinition(Actor);
}

UPangeaCreatureDefinition* UPangeaBreedingLibrary::GetCreatureDefinitionFromBreedable(const UPangeaBreedableComponent* Breedable)
{
	return Breedable != nullptr ? GetCreatureDefinitionFromActor(Breedable->GetOwner()) : nullptr;
}

UPangeaBreedingFragment* UPangeaBreedingLibrary::GetBreedingFragmentFromActor(const AActor* Actor)
{
	if (UPangeaCreatureDefinition* Definition = GetCreatureDefinitionFromActor(Actor))
	{
		return Definition->GetFragment<UPangeaBreedingFragment>();
	}

	return nullptr;
}

UPangeaBreedingFragment* UPangeaBreedingLibrary::GetBreedingFragmentFromBreedable(const UPangeaBreedableComponent* Breedable)
{
	return Breedable != nullptr ? GetBreedingFragmentFromActor(Breedable->GetOwner()) : nullptr;
}

TSubclassOf<APangeaEggActor> UPangeaBreedingLibrary::ResolveEggActorClass(
	const UPangeaBreedableComponent* Breedable, const TSubclassOf<APangeaEggActor> FallbackEggClass)
{
	if (const UPangeaBreedingFragment* BreedingFragment = GetBreedingFragmentFromBreedable(Breedable))
	{
		if (BreedingFragment->EggActorClass)
		{
			return BreedingFragment->EggActorClass;
		}
	}

	return FallbackEggClass;
}

UPangeaGeneticStrategy* UPangeaBreedingLibrary::CreateGeneticStrategy(
	UObject* Outer, const UPangeaBreedableComponent* Breedable, UPangeaGeneticStrategy* FallbackStrategy)
{
	if (const UPangeaBreedingFragment* BreedingFragment = GetBreedingFragmentFromBreedable(Breedable))
	{
		if (BreedingFragment->GeneticStrategyClass)
		{
			UObject* const StrategyOuter = Outer != nullptr ? Outer : GetTransientPackage();
			return NewObject<UPangeaGeneticStrategy>(StrategyOuter, BreedingFragment->GeneticStrategyClass);
		}
	}

	return FallbackStrategy;
}
