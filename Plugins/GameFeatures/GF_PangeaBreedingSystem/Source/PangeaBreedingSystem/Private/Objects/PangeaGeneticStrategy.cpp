// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Objects/PangeaGeneticStrategy.h"

FGeneticTraitSet UPangeaGeneticStrategy::CombineTraits_Implementation(const FParentSnapshot& ParentA, const FParentSnapshot& ParentB) const
{
	FGeneticTraitSet Out;
	TSet<FName> Keys;

	for (const FGeneticTrait& Trait : ParentA.Traits.Traits)
	{
		Keys.Add(Trait.Name);
	}

	for (const FGeneticTrait& Trait : ParentB.Traits.Traits)
	{
		Keys.Add(Trait.Name);
	}

	for (const FName& Key : Keys)
	{
		const float ParentAValue = ParentA.Traits.GetValue(Key);
		const float ParentBValue = ParentB.Traits.GetValue(Key);
		const float Base = (ParentAValue + ParentBValue) * 0.5f;
		const float Mutation = FMath::FRandRange(-0.05f, 0.05f) * Base;
		Out.SetValue(Key, Base + Mutation);
	}

	return Out;
}
