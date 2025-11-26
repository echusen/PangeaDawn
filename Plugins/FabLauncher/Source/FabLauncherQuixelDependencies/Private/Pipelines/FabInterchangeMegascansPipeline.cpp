// Copyright Epic Games, Inc. All Rights Reserved.
#include "FabInterchangeMegascansPipeline.h"
#include "IFabLauncherQuixelDependenciesModule.h"

#include "FoliageType.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeMaterialInstanceNode.h"
#include "InterchangeMeshFactoryNode.h"
#include "InterchangeMeshNode.h"
#if ENGINE_MAJOR_VERSION== 5 && ENGINE_MINOR_VERSION >= 4
#include "InterchangePipelineHelper.h"
#endif
#include "InterchangePipelineMeshesUtilities.h"
#include "InterchangeSceneNode.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeStaticMeshLodDataNode.h"
#include "InterchangeTextureFactoryNode.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "Materials/MaterialInstanceConstant.h"

#include "Nodes/FabInterchangeInstancedFoliageTypeFactoryNode.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"
#include "Nodes/InterchangeSourceNode.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonTypes.h"

#define MEGASCAN_BASE_KEY									TEXT("Megascan")

#define MEGASCAN_MATERIAL_KEY							    MEGASCAN_BASE_KEY TEXT(".Material")

#define MEGASCAN_MATERIAL_TYPE_KEY							MEGASCAN_MATERIAL_KEY TEXT(".Type")

#define MEGASCAN_MATERIAL_BLEND_MODE_KEY				    MEGASCAN_MATERIAL_KEY TEXT(".BlendMode")
#define MEGASCAN_MATERIAL_BLEND_MODE_VALUE_KEY				MEGASCAN_MATERIAL_BLEND_MODE_KEY TEXT(".Value")
#define MEGASCAN_MATERIAL_BLEND_MODE_OVERRIDE_KEY		    MEGASCAN_MATERIAL_BLEND_MODE_KEY TEXT(".Override")

#define MEGASCAN_MATERIAL_DISPLACEMENT_KEY					MEGASCAN_MATERIAL_KEY TEXT(".Displacement")
#define MEGASCAN_MATERIAL_DISPLACEMENT_OVERRIDE_KEY			MEGASCAN_MATERIAL_DISPLACEMENT_KEY TEXT(".Override")
#define MEGASCAN_MATERIAL_DISPLACEMENT_MAGNITUDE_KEY		MEGASCAN_MATERIAL_DISPLACEMENT_KEY TEXT(".Magnitude")
#define MEGASCAN_MATERIAL_DISPLACEMENT_CENTER_KEY			MEGASCAN_MATERIAL_DISPLACEMENT_KEY TEXT(".Center")

#define MEGASCAN_MESH_KEY									MEGASCAN_BASE_KEY TEXT(".Mesh")
#define MEGASCAN_MESH_GENERATE_DISTANCE_FIELD_KEY			MEGASCAN_MESH_KEY TEXT(".GenerateDistanceField")
#define MEGASCAN_MESH_AUTO_COMPUTE_LOD_SCREEN_SIZE_KEY		MEGASCAN_MESH_KEY TEXT(".AutoComputeLODScreenSize")
#define MEGASCAN_MESH_NANITE_SETTINGS_KEY				    MEGASCAN_MESH_KEY TEXT(".Nanite")
#define MEGASCAN_MESH_NANITE_PRESERVE_AREA_KEY				MEGASCAN_MESH_NANITE_SETTINGS_KEY TEXT(".PreserveArea")

UFabInterchangeMegascansPipeline::UFabInterchangeMegascansPipeline()
	: MegascanImportType(EFabMegascanImportType::Model3D)
{}

void UFabInterchangeMegascansPipeline::ExecutePipeline(
	UInterchangeBaseNodeContainer* NodeContainer,
	const TArray<UInterchangeSourceData*>& SourceDatas
	#if ENGINE_MAJOR_VERSION== 5 && ENGINE_MINOR_VERSION > 3
	,
	const FString& ContentBasePath
	#endif
)
{
	Super::ExecutePipeline(
		NodeContainer,
		SourceDatas
		#if ENGINE_MAJOR_VERSION== 5 && ENGINE_MINOR_VERSION > 3
		,
		ContentBasePath
		#endif
	);

	BaseNodeContainer = NodeContainer;

	const UInterchangeSourceData* const* GltfSourceData = SourceDatas.FindByPredicate(
		[](const UInterchangeSourceData* SourceData)
		{
			return FPaths::GetExtension(SourceData->GetFilename()) == "gltf";
		}
	);
	if (!GltfSourceData)
	{
		return;
	}

	if (!LoadGltfSource((*GltfSourceData)->GetFilename()))
	{
		return;
	}

	if (const TSharedPtr<FJsonObject>* GltfExtras; GltfJson->TryGetObjectField(TEXT("extras"), GltfExtras))
	{
		GltfExtras->Get()->TryGetNumberField(TEXT("tier"), reinterpret_cast<int8&>(MegascanAssetTier));
	}

	TextureFactoryNodes          = GetNodesOfType<UInterchangeTextureFactoryNode>();
	StaticMeshFactoryNodes       = GetNodesOfType<UInterchangeStaticMeshFactoryNode>();
	MaterialInstanceFactoryNodes = GetNodesOfType<UInterchangeMaterialInstanceFactoryNode>();

	ForEachGltfTexture(
		[&](const FString& TextureName, const TSharedPtr<FJsonObject>& Texture)
		{
			UInterchangeTextureFactoryNode* TextureFactoryNode = FindTextureFactoryNodeByName(TextureName);
			if (TextureFactoryNode == nullptr)
			{
				return;
			}

			if (const TSharedPtr<FJsonObject>* TextureExtras; Texture->TryGetObjectField(TEXT("extras"), TextureExtras))
			{
				SetupTextureParams(TextureFactoryNode, *TextureExtras);
			}
		}
	);

	ForEachGltfMaterial(
		[&](const FString& MaterialName, const TSharedPtr<FJsonObject>& Material)
		{
			FString MaterialType;
			if (MegascanImportType == EFabMegascanImportType::Plant && MaterialName.EndsWith("_Billboard", ESearchCase::CaseSensitive))
				MaterialType = "billboard";

			UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode = FindMaterialInstanceFactoryNodeByName(MaterialName);
			if (MaterialInstanceFactoryNode == nullptr)
			{
				return;
			}

			SetupMaterial(MaterialInstanceFactoryNode);
			if (const TSharedPtr<FJsonObject>* MaterialExtras; Material->TryGetObjectField(TEXT("extras"), MaterialExtras))
			{
				MaterialExtras->Get()->TryGetStringField(TEXT("type"), MaterialType);
				SetupMaterialParams(MaterialInstanceFactoryNode, *MaterialExtras);
			}
			SetupMaterialParents(MaterialInstanceFactoryNode, MaterialType);
		}
	);

	ForEachGltfMesh(
		[&](const FString& MeshName, const TSharedPtr<FJsonObject>& Mesh)
		{
			UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = FindStaticMeshFactoryNodeByName(MeshName);
			if (StaticMeshFactoryNode == nullptr)
			{
				return;
			}

			SetupStaticMesh(StaticMeshFactoryNode);
			if (const TSharedPtr<FJsonObject>* MeshExtras; Mesh->TryGetObjectField(TEXT("extras"), MeshExtras))
			{
				SetupStaticMeshParams(StaticMeshFactoryNode, *MeshExtras);
			}
		}
	);

	StaticMeshFactoryNodes = GetNodesOfType<UInterchangeStaticMeshFactoryNode>();
	for (UInterchangeStaticMeshFactoryNode* MeshFactoryNode : StaticMeshFactoryNodes)
	{
		UE::Interchange::MeshesUtilities::ReorderSlotMaterialDependencies(*MeshFactoryNode, *BaseNodeContainer);
	}
}

void UFabInterchangeMegascansPipeline::ExecutePostFactoryPipeline(
	const UInterchangeBaseNodeContainer* NodeContainer,
	const FString& NodeKey,
	UObject* CreatedAsset,
	const bool bIsAReimport
)
{
	Super::ExecutePostFactoryPipeline(NodeContainer, NodeKey, CreatedAsset, bIsAReimport);

	if (MegascanImportType == EFabMegascanImportType::Plant && MegascanAssetTier > EFabMegascanImportTier::Raw)
	{
		if (UStaticMesh* ImportedMesh = Cast<UStaticMesh>(CreatedAsset))
		{
			const float BillboardScreenSizes[] = {
				0.03f,
				0.05f,
				0.10f
			};
			int32 Index = 0;
			for (const int32 MaxIndex = ImportedMesh->GetNumSourceModels(); Index < MaxIndex; ++Index)
			{
				ImportedMesh->GetSourceModel(Index).ScreenSize = FMath::Pow(0.75f, Index);
			}
			if (const int ScreenSizeIndex = static_cast<int8>(MegascanAssetTier) - 1; ScreenSizeIndex >= 0)
				ImportedMesh->GetSourceModel(Index - 1).ScreenSize = BillboardScreenSizes[ScreenSizeIndex];
		}
	}

	if (bVirtualTexturesImported)
	{
		return;
	}

	if (const UTexture* ImportedTexture = Cast<UTexture>(CreatedAsset))
	{
		bVirtualTexturesImported |= ImportedTexture->VirtualTextureStreaming;
	}

	if (bVirtualTexturesImported)
	{
		for (UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode : MaterialInstanceFactoryNodes)
		{
			UpdateParentMaterial(MaterialInstanceFactoryNode, true, true);
		}
	}
}

bool UFabInterchangeMegascansPipeline::LoadGltfSource(const FString& SourceFile)
{
	if (FString GltfFileData; FFileHelper::LoadFileToString(GltfFileData, *SourceFile))
	{
		GltfJson = MakeShareable(new FJsonObject);
		return FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(GltfFileData), GltfJson);
	}
	return false;
}

void UFabInterchangeMegascansPipeline::ForEachGltfMaterial(const TFunction<void(const FString&, const TSharedPtr<FJsonObject>&)>& Callback) const
{
	if (GltfJson == nullptr) { return; }
	const TArray<TSharedPtr<FJsonValue>>& Materials = GltfJson->GetArrayField(TEXT("materials"));
	for (const TSharedPtr<FJsonValue>& Material : Materials)
	{
		const TSharedPtr<FJsonObject>& MaterialObject = Material->AsObject();
		const FString MaterialName                    = MaterialObject->GetStringField(TEXT("name"));
		Callback(MaterialName, MaterialObject);
	}
}

void UFabInterchangeMegascansPipeline::ForEachGltfTexture(const TFunction<void(const FString&, const TSharedPtr<FJsonObject>&)>& Callback)
{
	if (GltfJson == nullptr) { return; }
	const TArray<TSharedPtr<FJsonValue>>& Images = GltfJson->GetArrayField(TEXT("images"));
	for (const TSharedPtr<FJsonValue>& Image : Images)
	{
		const TSharedPtr<FJsonObject>& ImageObject = Image->AsObject();
		const FString ImageName                    = ImageObject->GetStringField(TEXT("name"));
		Callback(ImageName, ImageObject);
	}
}

void UFabInterchangeMegascansPipeline::ForEachGltfMesh(const TFunction<void(const FString&, const TSharedPtr<FJsonObject>&)>& Callback)
{
	if (GltfJson == nullptr) { return; }
	const TArray<TSharedPtr<FJsonValue>>& MeshNodes = GltfJson->GetArrayField(TEXT("nodes"));
	for (const TSharedPtr<FJsonValue>& MeshNode : MeshNodes)
	{
		const TSharedPtr<FJsonObject>& MeshNodeObject = MeshNode->AsObject();
		const FString MeshNodeName                    = MeshNodeObject->GetStringField(TEXT("name"));
		Callback(MeshNodeName, MeshNodeObject);
	}
}

UInterchangeTextureFactoryNode* UFabInterchangeMegascansPipeline::FindTextureFactoryNodeByName(const FString& DisplayName) const
{
	UInterchangeTextureFactoryNode* const* const FoundNode = TextureFactoryNodes.FindByPredicate(
		[&DisplayName](const UInterchangeTextureFactoryNode* Node)
		{
			return Node->GetDisplayLabel() == DisplayName;
		}
	);
	return FoundNode ? *FoundNode : nullptr;
}

UInterchangeStaticMeshFactoryNode* UFabInterchangeMegascansPipeline::FindStaticMeshFactoryNodeByName(const FString& DisplayName) const
{
	UInterchangeStaticMeshFactoryNode* const* const FoundNode = StaticMeshFactoryNodes.FindByPredicate(
		[&DisplayName](const UInterchangeStaticMeshFactoryNode* Node)
		{
			return Node->GetDisplayLabel() == DisplayName;
		}
	);
	return FoundNode ? *FoundNode : nullptr;
}

UInterchangeMaterialInstanceFactoryNode* UFabInterchangeMegascansPipeline::FindMaterialInstanceFactoryNodeByName(const FString& DisplayName) const
{
	UInterchangeMaterialInstanceFactoryNode* const* const FoundNode = MaterialInstanceFactoryNodes.FindByPredicate(
		[&DisplayName](const UInterchangeMaterialInstanceFactoryNode* Node)
		{
			return Node->GetDisplayLabel() == DisplayName;
		}
	);
	return FoundNode ? *FoundNode : nullptr;
}

const TSharedPtr<FJsonObject>* UFabInterchangeMegascansPipeline::GetMaterialAtIndex(uint32 Index) const
{
	TSharedPtr<FJsonObject>* Material              = nullptr;
	const TArray<TSharedPtr<FJsonValue>> Materials = GltfJson->GetArrayField(TEXT("materials"));
	if (Materials.IsValidIndex(Index))
	{
		Materials[Index]->TryGetObject(Material);
	}
	return Material;
}

EFabMegascanMaterialType UFabInterchangeMegascansPipeline::GetMegascanMaterialType(const UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode) const
{
	int32 MaterialType = 0;
	MaterialInstanceFactoryNode->GetInt32Attribute(MEGASCAN_MATERIAL_TYPE_KEY, MaterialType);

	return static_cast<EFabMegascanMaterialType>(MaterialType);
}

bool UFabInterchangeMegascansPipeline::SetMegascanMaterialType(UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode, EFabMegascanMaterialType MaterialType) const
{
	return MaterialInstanceFactoryNode->AddInt32Attribute(MEGASCAN_MATERIAL_TYPE_KEY, static_cast<int32>(MaterialType));
}

bool UFabInterchangeMegascansPipeline::UpdateParentMaterial(UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode, bool bVTMaterial, bool bUpdateReferencedObject)
{
	const EFabMegascanMaterialType MaterialType = GetMegascanMaterialType(MaterialInstanceFactoryNode);

	auto M = [](const FString& Path)
	{
		return TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Path));
	};

	// TODO : this has been harcoded for now, would need to go back to a ini file, or reuse Fab's version
	TMap<EFabMegascanMaterialType, FFabMegascanMaterialPair> MaterialParents = {
		{EFabMegascanMaterialType::Base, {M("/Fab/Materials/Standard/M_MS_Base.M_MS_Base"), M("/Fab/Materials/VT/M_MS_Base_VT.M_MS_Base_VT")}},
		{EFabMegascanMaterialType::BaseMasked, {M("/Fab/Materials/Standard/M_MS_Base_Masked.M_MS_Base_Masked"), M("/Fab/Materials/VT/M_MS_Base_Masked_VT.M_MS_Base_Masked_VT")}},
		{EFabMegascanMaterialType::BaseFuzz, {M("/Fab/Materials/Standard/M_MS_Base_Fuzz.M_MS_Base_Fuzz"), M("/Fab/Materials/VT/M_MS_Base_Fuzz_VT.M_MS_Base_Fuzz_VT")}},
		{EFabMegascanMaterialType::BaseTransmission, {M("/Fab/Materials/Standard/M_MS_Base_Trm.M_MS_Base_Trm"), M("/Fab/Materials/VT/M_MS_Base_Trm_VT.M_MS_Base_Trm_VT")}},
		{EFabMegascanMaterialType::Glass, {M("/Fab/Materials/Standard/M_MS_Glass.M_MS_Glass"), M("/Fab/Materials/VT/M_MS_Glass_VT.M_MS_Glass_VT")}},
		{EFabMegascanMaterialType::Surface, {M("/Fab/Materials/Standard/M_MS_Srf.M_MS_Srf"), M("/Fab/Materials/VT/M_MS_Srf_VT.M_MS_Srf_VT")}},
		{EFabMegascanMaterialType::SurfaceMasked, {M("/Fab/Materials/Standard/M_MS_Srf_Masked.M_MS_Srf_Masked"), M("/Fab/Materials/VT/M_MS_Srf_Masked_VT.M_MS_Srf_Masked_VT")}},
		{EFabMegascanMaterialType::SurfaceFuzz, {M("/Fab/Materials/Standard/M_MS_Srf_Fuzz.M_MS_Srf_Fuzz"), M("/Fab/Materials/VT/M_MS_Srf_Fuzz_VT.M_MS_Srf_Fuzz_VT")}},
		{EFabMegascanMaterialType::SurfaceTransmission, {M("/Fab/Materials/Standard/M_MS_Srf_Trm.M_MS_Srf_Trm"), M("/Fab/Materials/VT/M_MS_Srf_Trm_VT.M_MS_Srf_Trm_VT")}},
		{EFabMegascanMaterialType::Fabric, {M("/Fab/Materials/Standard/M_MS_Fabric.M_MS_Fabric"), M("/Fab/Materials/VT/M_MS_Fabric_VT.M_MS_Fabric_VT")}},
		{EFabMegascanMaterialType::FabricMasked, {M("/Fab/Materials/Standard/M_MS_Fabric_Masked.M_MS_Fabric_Masked"), M("/Fab/Materials/VT/M_MS_Fabric_Masked_VT.M_MS_Fabric_Masked_VT")}},
		{EFabMegascanMaterialType::Decal, {M("/Fab/Materials/Standard/M_MS_Decal.M_MS_Decal"), M("/Fab/Materials/VT/M_MS_Decal_VT.M_MS_Decal_VT")}},
		{EFabMegascanMaterialType::Plant, {M("/Fab/Materials/Standard/M_MS_Foliage.M_MS_Foliage"), M("/Fab/Materials/VT/M_MS_Foliage_VT.M_MS_Foliage_VT")}},
		{EFabMegascanMaterialType::PlantBillboard, {M("/Fab/Materials/Standard/M_MS_Billboard.M_MS_Billboard"), M("/Fab/Materials/VT/M_MS_Billboard_VT.M_MS_Billboard_VT")}}
	};

	if (const FFabMegascanMaterialPair* const ParentMaterialPair = MaterialParents.Find(MaterialType))
	{
		const TSoftObjectPtr<UMaterialInterface> ParentMaterial = bVTMaterial ? ParentMaterialPair->VTMaterial : ParentMaterialPair->StandardMaterial;

		if (bUpdateReferencedObject)
		{
			if (FSoftObjectPath MaterialInstancePath; MaterialInstanceFactoryNode->GetCustomReferenceObject(MaterialInstancePath))
			{
				UObject* ImportedObject = MaterialInstancePath.TryLoad();
				if (UMaterialInstanceConstant* Material = Cast<UMaterialInstanceConstant>(ImportedObject))
				{
					Material->SetParentEditorOnly(ParentMaterial.LoadSynchronous());
				}
			}
		}

		return MaterialInstanceFactoryNode->SetCustomParent(ParentMaterial.ToString());
	}
	else
	{
		UE_LOG(LogFabLauncherQuixelDependencies, Error, TEXT("Material type %d was not matched to a parent material"), static_cast<int32>(MaterialType));
	}
	return false;
}

FString UFabInterchangeMegascansPipeline::GetStaticMeshLodDataNodeUid(const UInterchangeMeshFactoryNode* MeshFactoryNode, int32 LodIndex)
{
	const FString MeshFactoryUid = MeshFactoryNode->GetUniqueID();
	const FString LODDataPrefix  = TEXT("\\LodData") + (LodIndex > 0 ? FString::FromInt(LodIndex) : TEXT(""));
	return LODDataPrefix + MeshFactoryUid;
}

FString UFabInterchangeMegascansPipeline::GetStaticMeshLodDataNodeDisplayName(int32 LodIndex)
{
	return TEXT("LodData") + FString::FromInt(LodIndex);
}

void UFabInterchangeMegascansPipeline::SetupMeshLod(UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode, const UInterchangeSceneNode* SceneNode, int32 LodIndex)
{
	const FString StaticMeshFactoryUid     = StaticMeshFactoryNode->GetUniqueID();
	const FString StaticMeshLodDataNodeUid = GetStaticMeshLodDataNodeUid(StaticMeshFactoryNode, LodIndex);

	UInterchangeStaticMeshLodDataNode* StaticMeshLodDataNode = Cast<UInterchangeStaticMeshLodDataNode>(BaseNodeContainer->GetFactoryNode(StaticMeshLodDataNodeUid));
	if (StaticMeshLodDataNode == nullptr)
	{
		StaticMeshLodDataNode = NewObject<UInterchangeStaticMeshLodDataNode>(BaseNodeContainer, NAME_None);
		#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)
		BaseNodeContainer->SetupNode(StaticMeshLodDataNode, StaticMeshLodDataNodeUid, GetStaticMeshLodDataNodeDisplayName(LodIndex), EInterchangeNodeContainerType::FactoryData, StaticMeshFactoryUid);
		#else
		StaticMeshLodDataNode->InitializeNode(StaticMeshLodDataNodeUid, GetStaticMeshLodDataNodeDisplayName(LodIndex), EInterchangeNodeContainerType::FactoryData);
		BaseNodeContainer->AddNode(StaticMeshLodDataNode);
		BaseNodeContainer->SetNodeParentUid(StaticMeshLodDataNodeUid, StaticMeshFactoryUid);
		#endif
		StaticMeshFactoryNode->AddLodDataUniqueId(StaticMeshLodDataNodeUid);
	}

	const FString SceneNodeUid = SceneNode->GetUniqueID();

	// UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(SceneNode, StaticMeshFactoryNode, true);
	StaticMeshFactoryNode->AddTargetNodeUid(SceneNodeUid);
	StaticMeshLodDataNode->AddMeshUid(SceneNodeUid);
	SceneNode->AddTargetNodeUid(StaticMeshFactoryUid);

	FString MeshNodeUid;
	SceneNode->GetCustomAssetInstanceUid(MeshNodeUid);
	if (const UInterchangeMeshNode* MeshNode = Cast<UInterchangeMeshNode>(BaseNodeContainer->GetNode(MeshNodeUid)))
	{
		TMap<FString, FString> SlotMaterialDependencies;
		MeshNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
		UE::Interchange::MeshesUtilities::ApplySlotMaterialDependencies(
			*StaticMeshFactoryNode,
			SlotMaterialDependencies,
			*BaseNodeContainer
			#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
			,
			nullptr
			#endif
		);
	}
}

void UFabInterchangeMegascansPipeline::SetFoliageType(const UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode)
{
	const FString StaticMeshFactoryNodeUid    = StaticMeshFactoryNode->GetUniqueID();
	const FString FoliageTypeNodeUid          = UFabInterchangeInstancedFoliageTypeFactoryNode::GetNodeUidFromStaticMeshFactoryUid(StaticMeshFactoryNodeUid);
	const FString FoliageTypeNodeDisplayLabel = StaticMeshFactoryNode->GetDisplayLabel().Replace(TEXT("SM_"), TEXT("FT_"), ESearchCase::CaseSensitive);

	UFabInterchangeInstancedFoliageTypeFactoryNode* InstancedFoliageTypeFactoryNode = NewObject<UFabInterchangeInstancedFoliageTypeFactoryNode>(BaseNodeContainer, NAME_None);

	#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >=6
	BaseNodeContainer->SetupNode(InstancedFoliageTypeFactoryNode, FoliageTypeNodeUid, FoliageTypeNodeDisplayLabel, EInterchangeNodeContainerType::FactoryData, StaticMeshFactoryNodeUid);
	UInterchangeSourceNode* SourceNode = UInterchangeSourceNode::FindOrCreateUniqueInstance(BaseNodeContainer);
	UE::Interchange::PipelineHelper::FillSubPathFromSourceNode(InstancedFoliageTypeFactoryNode, SourceNode);
	#else
	InstancedFoliageTypeFactoryNode->InitializeNode(FoliageTypeNodeUid, FoliageTypeNodeDisplayLabel, EInterchangeNodeContainerType::FactoryData);
	BaseNodeContainer->AddNode(InstancedFoliageTypeFactoryNode);
	BaseNodeContainer->SetNodeParentUid(FoliageTypeNodeUid, StaticMeshFactoryNodeUid);
	#endif

	StaticMeshFactoryNode->AddTargetNodeUid(FoliageTypeNodeUid);
	InstancedFoliageTypeFactoryNode->AddTargetNodeUid(StaticMeshFactoryNodeUid);

	InstancedFoliageTypeFactoryNode->AddFactoryDependencyUid(StaticMeshFactoryNodeUid);

	InstancedFoliageTypeFactoryNode->SetCustomSubPath("FoliageTypes");
	InstancedFoliageTypeFactoryNode->SetCustomStaticMesh(StaticMeshFactoryNodeUid);
	InstancedFoliageTypeFactoryNode->SetCustomScaling(EFoliageScaling::Free);
	InstancedFoliageTypeFactoryNode->SetCustomScaleX(FVector2f(0.8f, 1.2f));
	InstancedFoliageTypeFactoryNode->SetCustomScaleY(FVector2f(0.8f, 1.2f));
	InstancedFoliageTypeFactoryNode->SetCustomScaleZ(FVector2f(0.8f, 1.2f));
	InstancedFoliageTypeFactoryNode->SetCustomAlignToNormal(false);
	InstancedFoliageTypeFactoryNode->SetCustomRandomYaw(true);
	InstancedFoliageTypeFactoryNode->SetCustomRandomPitchAngle(3.0f);
	InstancedFoliageTypeFactoryNode->SetCustomAffectDistanceFieldLighting(false);
	if (MegascanAssetTier != EFabMegascanImportTier::Invalid)
		InstancedFoliageTypeFactoryNode->SetCustomWorldPositionOffsetDisableDistance(5000 - (1000 * static_cast<int8>(MegascanAssetTier)));
}

void UFabInterchangeMegascansPipeline::SetupTextureParams(UInterchangeTextureFactoryNode* TextureFactoryNode, const TSharedPtr<FJsonObject>& TextureParams)
{
	if (FString CompressionSettings; TextureParams->TryGetStringField(TEXT("compression"), CompressionSettings))
	{
		if (CompressionSettings == "mask")
			TextureFactoryNode->SetCustomCompressionSettings(TC_Masks);
		else if (CompressionSettings == "displacement" || CompressionSettings == "alpha")
			TextureFactoryNode->SetCustomCompressionSettings(TC_Alpha);
	}

	if (FString MigGenSettings; TextureParams->TryGetStringField(TEXT("mipgen"), MigGenSettings))
	{
		if (MigGenSettings == "sharpen_4")
			TextureFactoryNode->SetCustomMipGenSettings(TMGS_Sharpen4);
		else if (MigGenSettings == "sharpen_6")
			TextureFactoryNode->SetCustomMipGenSettings(TMGS_Sharpen6);
	}

	if (const TArray<TSharedPtr<FJsonValue>>* AlphaCoverage; TextureParams->TryGetArrayField(TEXT("alphaCoverage"), AlphaCoverage))
	{
		if (AlphaCoverage->Num() >= 4)
		{
			TextureFactoryNode->SetCustomAlphaCoverageThresholds(
				FVector4(
					AlphaCoverage->GetData()[0]->AsNumber(),
					AlphaCoverage->GetData()[1]->AsNumber(),
					AlphaCoverage->GetData()[2]->AsNumber(),
					AlphaCoverage->GetData()[3]->AsNumber()
				)
			);
		}
	}

	if (bool bScaleMips; TextureParams->TryGetBoolField(TEXT("scaleMips"), bScaleMips))
	{
		TextureFactoryNode->SetCustombDoScaleMipsForAlphaCoverage(bScaleMips);
	}

	if (FString TextureSlot; TextureParams->TryGetStringField(TEXT("textureSlot"), TextureSlot))
	{
		int32 MaterialIndex = 0;
		TextureParams->TryGetNumberField(TEXT("materialIndex"), MaterialIndex);
		if (const TSharedPtr<FJsonObject>* MaterialObject = GetMaterialAtIndex(MaterialIndex))
		{
			const FString MaterialName = MaterialObject->Get()->GetStringField(TEXT("name"));
			if (UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode = FindMaterialInstanceFactoryNodeByName(MaterialName))
			{
				const FString ParameterName     = UInterchangeShaderPortsAPI::MakeInputValueKey(TextureSlot);
				const FString TextureFactoryUid = TextureFactoryNode->GetUniqueID();
				MaterialInstanceFactoryNode->AddStringAttribute(ParameterName, TextureFactoryUid);
				MaterialInstanceFactoryNode->AddFactoryDependencyUid(TextureFactoryUid);
			}
		}
	}
}

void UFabInterchangeMegascansPipeline::SetupStaticMesh(UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode)
{
	if (MegascanImportType == EFabMegascanImportType::Plant)
	{
		StaticMeshFactoryNode->SetCustomMinLightmapResolution(128);
		StaticMeshFactoryNode->SetAttribute(MEGASCAN_MESH_GENERATE_DISTANCE_FIELD_KEY, true);
		StaticMeshFactoryNode->AddApplyAndFillDelegates<bool>(
			MEGASCAN_MESH_GENERATE_DISTANCE_FIELD_KEY,
			StaticMeshFactoryNode->GetObjectClass(),
			"bGenerateMeshDistanceField"
		);

		if (MegascanAssetTier == EFabMegascanImportTier::Raw)
		{
			StaticMeshFactoryNode->SetAttribute(MEGASCAN_MESH_NANITE_PRESERVE_AREA_KEY, true);
			StaticMeshFactoryNode->AddApplyAndFillDelegates<bool>(
				MEGASCAN_MESH_NANITE_PRESERVE_AREA_KEY,
				StaticMeshFactoryNode->GetObjectClass(),
				"NaniteSettings.bPreserveArea"
			);
		}
		else
		{
			StaticMeshFactoryNode->SetAttribute(MEGASCAN_MESH_AUTO_COMPUTE_LOD_SCREEN_SIZE_KEY, false);
			StaticMeshFactoryNode->AddApplyAndFillDelegates<bool>(
				MEGASCAN_MESH_AUTO_COMPUTE_LOD_SCREEN_SIZE_KEY,
				StaticMeshFactoryNode->GetObjectClass(),
				"bAutoComputeLODScreenSize"
			);
		}

		const FString StaticMeshDisplayName = StaticMeshFactoryNode->GetDisplayLabel();
		if (!StaticMeshDisplayName.Contains(TEXT("_LOD"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			SetFoliageType(StaticMeshFactoryNode);
		}
	}
}

void UFabInterchangeMegascansPipeline::SetupStaticMeshParams(const UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode, const TSharedPtr<FJsonObject>& MeshParams)
{
	if (const TSharedPtr<FJsonObject>* LodInfo; MeshParams->TryGetObjectField(TEXT("lod"), LodInfo))
	{
		const FString StaticMeshFactoryUid = StaticMeshFactoryNode->GetUniqueID();
		const int32 LodIndex               = LodInfo->Get()->GetNumberField(TEXT("index"));
		const FString LodMeshName          = LodInfo->Get()->GetStringField(TEXT("mesh"));

		UInterchangeStaticMeshFactoryNode* ParentStaticMeshFactoryNode = FindStaticMeshFactoryNodeByName(LodMeshName);
		if (ParentStaticMeshFactoryNode == nullptr)
		{
			return;
		}

		const UInterchangeSceneNode* SceneNode = FindNodeOfTypeByName<UInterchangeSceneNode>(StaticMeshFactoryNode->GetDisplayLabel());
		SetupMeshLod(ParentStaticMeshFactoryNode, SceneNode, LodIndex);

		BaseNodeContainer->ReplaceNode(StaticMeshFactoryUid, nullptr);
	}
}

void UFabInterchangeMegascansPipeline::SetupMaterial(UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode)
{
	if (MegascanImportType == EFabMegascanImportType::Plant && MegascanAssetTier == EFabMegascanImportTier::Raw)
	{
		MaterialInstanceFactoryNode->SetAttribute(MEGASCAN_MATERIAL_BLEND_MODE_OVERRIDE_KEY, true);
		MaterialInstanceFactoryNode->AddApplyAndFillDelegates<bool>(
			MEGASCAN_MATERIAL_BLEND_MODE_OVERRIDE_KEY,
			UMaterialInstanceConstant::StaticClass(),
			"BasePropertyOverrides.bOverride_BlendMode"
		);
		MaterialInstanceFactoryNode->SetAttribute(MEGASCAN_MATERIAL_BLEND_MODE_VALUE_KEY, static_cast<int>(BLEND_Opaque));
		MaterialInstanceFactoryNode->AddApplyAndFillDelegates<int>(
			MEGASCAN_MATERIAL_BLEND_MODE_VALUE_KEY,
			MaterialInstanceFactoryNode->GetObjectClass(),
			"BasePropertyOverrides.BlendMode"
		);
	}
}

void UFabInterchangeMegascansPipeline::SetupMaterialParams(UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode, const TSharedPtr<FJsonObject>& MaterialParams)
{
	if (const TSharedPtr<FJsonObject>* Overrides; MaterialParams->TryGetObjectField(TEXT("overrides"), Overrides))
	{
		if (const TSharedPtr<FJsonObject>* Displacement; Overrides->Get()->TryGetObjectField(TEXT("displacement"), Displacement))
		{
			MaterialInstanceFactoryNode->SetAttribute(MEGASCAN_MATERIAL_DISPLACEMENT_OVERRIDE_KEY, true);
			MaterialInstanceFactoryNode->AddApplyAndFillDelegates<bool>(
				MEGASCAN_MATERIAL_DISPLACEMENT_OVERRIDE_KEY,
				UMaterialInstanceConstant::StaticClass(),
				"BasePropertyOverrides.bOverride_DisplacementScaling"
			);

			const float Magnitude = Displacement->Get()->GetNumberField(TEXT("magnitude"));
			MaterialInstanceFactoryNode->SetAttribute(MEGASCAN_MATERIAL_DISPLACEMENT_MAGNITUDE_KEY, Magnitude);
			MaterialInstanceFactoryNode->AddApplyAndFillDelegates<float>(
				MEGASCAN_MATERIAL_DISPLACEMENT_MAGNITUDE_KEY,
				UMaterialInstanceConstant::StaticClass(),
				"BasePropertyOverrides.DisplacementScaling.Magnitude"
			);

			const float Center = Displacement->Get()->GetNumberField(TEXT("center"));
			MaterialInstanceFactoryNode->SetAttribute(MEGASCAN_MATERIAL_DISPLACEMENT_CENTER_KEY, Center);
			MaterialInstanceFactoryNode->AddApplyAndFillDelegates<float>(
				MEGASCAN_MATERIAL_DISPLACEMENT_CENTER_KEY,
				UMaterialInstanceConstant::StaticClass(),
				"BasePropertyOverrides.DisplacementScaling.Center"
			);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* CustomParams;
	if (!MaterialParams->TryGetArrayField(TEXT("params"), CustomParams))
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Param : *CustomParams)
	{
		if (Param->Type != EJson::Object)
			continue;

		const TSharedPtr<FJsonObject>& ParamObject = Param->AsObject();
		const FString Name                         = ParamObject->GetStringField(TEXT("Name"));
		const TSharedPtr<FJsonValue> Value         = ParamObject->TryGetField(TEXT("Value"));

		const FString ParameterName = UInterchangeShaderPortsAPI::MakeInputValueKey(Name);
		if (Value->Type == EJson::Boolean)
		{
			MaterialInstanceFactoryNode->AddBooleanAttribute(ParameterName, Value->AsBool());
		}
		else if (Value->Type == EJson::Number)
		{
			MaterialInstanceFactoryNode->AddFloatAttribute(ParameterName, Value->AsNumber());
		}
		else if (Value->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& ArrayValue = Value->AsArray();
			if (ArrayValue.Num() >= 4)
			{
				MaterialInstanceFactoryNode->AddLinearColorAttribute(
					ParameterName,
					FLinearColor(
						ArrayValue[0]->AsNumber(),
						ArrayValue[1]->AsNumber(),
						ArrayValue[2]->AsNumber(),
						ArrayValue[3]->AsNumber()
					)
				);
			}
		}
	}
}

void UFabInterchangeMegascansPipeline::SetupMaterialParents(UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode, const FString& CustomType)
{
	switch (MegascanImportType)
	{
		case EFabMegascanImportType::Model3D:
		{
			if (CustomType.IsEmpty() || CustomType == "base")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::Base);
			else if (CustomType == "masked")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::BaseMasked);
			else if (CustomType == "transmission")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::BaseTransmission);
			else if (CustomType == "fuzz")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::BaseFuzz);
			else if (CustomType == "glass")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::Glass);
		}
		break;
		case EFabMegascanImportType::Surface:
		{
			if (CustomType.IsEmpty() || CustomType == "surface")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::Surface);
			else if (CustomType == "masked")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::SurfaceMasked);
			else if (CustomType == "transmission")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::SurfaceTransmission);
			else if (CustomType == "fuzz")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::SurfaceFuzz);
			else if (CustomType == "fabric" || CustomType == "fabric_opaque")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::Fabric);
			else if (CustomType == "fabric_masked")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::FabricMasked);
		}
		break;
		case EFabMegascanImportType::Decal:
		{
			if (CustomType.IsEmpty() || CustomType == "decal")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::Decal);
		}
		break;
		case EFabMegascanImportType::Plant:
		{
			if (CustomType.IsEmpty() || CustomType == "plant")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::Plant);
			else if (CustomType == "billboard")
				SetMegascanMaterialType(MaterialInstanceFactoryNode, EFabMegascanMaterialType::PlantBillboard);
		}
		break;

		default:
			break;
	}

	UpdateParentMaterial(MaterialInstanceFactoryNode);
}