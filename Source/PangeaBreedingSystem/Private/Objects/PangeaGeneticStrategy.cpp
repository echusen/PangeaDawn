// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.


#include "Objects/PangeaGeneticStrategy.h"

FGeneticTraitSet UPangeaGeneticStrategy::CombineTraits_Implementation(const FParentSnapshot& ParentA, const FParentSnapshot& ParentB) const
{
    FGeneticTraitSet Out;
    TSet<FName> Keys;
    for (const FGeneticTrait& T : ParentA.Traits.Traits) Keys.Add(T.Name);
    for (const FGeneticTrait& T : ParentB.Traits.Traits) Keys.Add(T.Name);

    for (const FName& Key : Keys)
    {
        const float A = ParentA.Traits.GetValue(Key);
        const float B = ParentB.Traits.GetValue(Key);
        const float Base = (A + B) * 0.5f;
        const float Mutation = FMath::FRandRange(-0.05f, 0.05f) * Base;
        Out.SetValue(Key, Base + Mutation);
    }
    return Out;
}