// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.


#include "Data/ACFCharacterDataAsset.h"
#include "Engine/SkeletalMesh.h"
#include <Engine/SkinnedAssetCommon.h>
#include <Materials/MaterialInterface.h>
#include <Data/ACFCharacterFragment.h>


UACFCharacterDataAsset::UACFCharacterDataAsset()
{
	MeshComponents.Add(FSkeletalMeshComponentData());
	MeshComponents[0].ComponentTag = FName("Mesh");
}

#if WITH_EDITOR
void UACFCharacterDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		FName PropertyName = PropertyChangedEvent.Property->GetFName();

		// Check if SkeletalMesh property was changed
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FSkeletalMeshComponentData, SkeletalMesh))
		{
			// Update material arrays for all mesh components
			for (FSkeletalMeshComponentData& MeshComponent : MeshComponents)
			{
				if (MeshComponent.SkeletalMesh)
				{
					UpdateMaterialArray(MeshComponent);
				}
			}
		}
	}
}

void UACFCharacterDataAsset::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// Handle array operations (add/remove/duplicate) for mesh components
	if (PropertyChangedEvent.Property)
	{
		FName PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UACFCharacterDataAsset, MeshComponents))
		{
			// Update material arrays for all mesh components
			for (FSkeletalMeshComponentData& MeshComponent : MeshComponents)
			{
				if (MeshComponent.SkeletalMesh)
				{
					UpdateMaterialArray(MeshComponent);
				}
			}
		}
	}
}

void UACFCharacterDataAsset::UpdateMaterialArray(FSkeletalMeshComponentData& MeshData)
{
	if (!MeshData.SkeletalMesh)
	{
		// Clear materials if no mesh is selected
		MeshData.MaterialOverrides.Empty();
		return;
	}

	// Get material slots from the skeletal mesh
	const TArray<FSkeletalMaterial>& MeshMaterials = MeshData.SkeletalMesh->GetMaterials();

	// Store existing material overrides to preserve user selections
	TArray<FMaterialOverrideData> ExistingOverrides = MeshData.MaterialOverrides;

	// Resize array to match mesh material count exactly
	MeshData.MaterialOverrides.Empty();
	MeshData.MaterialOverrides.SetNum(MeshMaterials.Num());

	for (int32 i = 0; i < MeshMaterials.Num(); i++)
	{
		// Try to preserve existing override at this index if it was explicitly set
		if (i < ExistingOverrides.Num() && ExistingOverrides[i].Material != nullptr)
		{
			MeshData.MaterialOverrides[i] = ExistingOverrides[i];
		}
		else
		{
			// Always populate with the mesh's default material
			// This ensures the array is never empty and shows defaults
			MeshData.MaterialOverrides[i].Material = MeshMaterials[i].MaterialInterface;
			MeshData.MaterialOverrides[i].SlotName = MeshMaterials[i].MaterialSlotName;
		}
	}

}
#endif

UACFCharacterFragment* UACFCharacterDataAsset::GetFragmentByClass(TSubclassOf<UACFCharacterFragment> FragmentClass) const
{
    if (!FragmentClass) {
        return nullptr;
    }

    for (UACFCharacterFragment* Fragment : Fragments) {
        if (Fragment && Fragment->IsA(FragmentClass)) {
            return Fragment;
        }
    }

    return nullptr;
}
