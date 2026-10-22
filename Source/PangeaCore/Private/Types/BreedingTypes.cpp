#include "Types/BreedingTypes.h"

namespace
{
// Private wire-format implementation, not a public Blueprint gameplay API.
constexpr uint32 MaxSnapshotEntries = 256;
bool SerializeCount(FArchive& Ar, uint32& Count)
{
	Ar.SerializeIntPacked(Count);
	if (Count > MaxSnapshotEntries) { Ar.SetError(); return false; }
	return !Ar.IsError();
}
template<typename T> bool SerializeNamedMap(FArchive& Ar, TMap<FName, T>& Values)
{
	uint32 Count = Ar.IsLoading() ? 0 : Values.Num();
	if (!SerializeCount(Ar, Count)) return false;
	if (Ar.IsLoading())
	{
		Values.Empty(Count);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FName Key; T Value{};
			Ar << Key << Value;
			Values.Add(Key, Value);
		}
	}
	else for (TPair<FName, T>& Pair : Values) Ar << Pair.Key << Pair.Value;
	return !Ar.IsError();
}
bool SerializeTaggedMap(FArchive& Ar, UPackageMap* Map, TMap<FGameplayTag, float>& Values)
{
	uint32 Count = Ar.IsLoading() ? 0 : Values.Num();
	if (!SerializeCount(Ar, Count)) return false;
	if (Ar.IsLoading()) Values.Empty(Count);
	const auto SerializePair = [&](FGameplayTag& Key, float& Value)
	{
		bool bTagSuccess = true;
		Key.NetSerialize(Ar, Map, bTagSuccess);
		Ar << Value;
		if (!bTagSuccess) Ar.SetError();
	};
	if (Ar.IsLoading())
	{
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FGameplayTag Key; float Value = 0;
			SerializePair(Key, Value);
			Values.Add(Key, Value);
		}
	}
	else for (TPair<FGameplayTag, float>& Pair : Values) SerializePair(Pair.Key, Pair.Value);
	return !Ar.IsError();
}
}

bool FParentSnapshot::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = false;
	Ar << SpeciesID << CreatureId;
	uint32 Count = Ar.IsLoading() ? 0 : Traits.Traits.Num();
	if (!SerializeCount(Ar, Count)) return false;
	if (Ar.IsLoading()) Traits.Traits.SetNum(Count);
	for (FGeneticTrait& Trait : Traits.Traits) Ar << Trait.Name << Trait.Value;
	if (Ar.IsLoading()) ParentActor.Reset();
	bOutSuccess = SerializeNamedMap(Ar, VisualData) && SerializeNamedMap(Ar, MaterialParams) &&
		SerializeNamedMap(Ar, ScalarParams) && SerializeTaggedMap(Ar, Map, InheritedStatistics) &&
		SerializeTaggedMap(Ar, Map, InheritedPrimaryAttributes) && SerializeTaggedMap(Ar, Map, InheritedAttributes) && !Ar.IsError();
	return bOutSuccess;
}
