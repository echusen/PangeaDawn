// Copyright Epic Games, Inc. All Rights Reserved.

#include "ACFSensorGridHelper.h"

#include "Algo/RemoveIf.h"
#include "GPUSort.h"
#include "NiagaraRenderGraphUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderPermutation.h"
#include "ShaderParameterStruct.h"
#include "GlobalShader.h"

namespace ACFSensorGridShaders
{
	static const uint32 SensorsPerOwnerAlignment = 128;
	static const uint32 MortonCompactionBufferSize = 128;
	static const uint32 MortonCodeBitsReservedForOwner = 5;

	// todo - look at collapsing to 16 bytes (2 x half3 + 2 x int16)
	struct alignas(16) FInternalNode
	{
		FVector BoundsMin;
		int32 LeftChild;

		FVector BoundsMax;
		int32 RightChild;
	};
};

static bool GACFSensorGridBuildDisabled = false;
static FAutoConsoleVariableRef CVarACFSensorGridBuildDisabled(
	TEXT("acf.sensorgrid.BuildDisabled"),
	GACFSensorGridBuildDisabled,
	TEXT("When true the sensor grid will not be build or traversed."),
	ECVF_Default
);

class FACFSensorGridResetSensorLocationsCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridResetSensorLocationsCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridResetSensorLocationsCs, FGlobalShader);

public:
	static const uint32 ChunkSize = 128;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, SensorsToReset)
		SHADER_PARAMETER(uint32, SensorCount)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("RESET_SENSOR_LOCATIONS_CS"), 1);
		OutEnvironment.SetDefine(TEXT("RESET_SENSOR_LOCATIONS_CHUNK_SIZE"), ChunkSize);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridResetSensorLocationsCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "ResetSensorLocations", SF_Compute);

class FACFSensorGridClearResultsCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridClearResultsCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridClearResultsCs, FGlobalShader);

public:
	static const uint32 ChunkSize = 128;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSensorInfo>, NearestSensors)
		SHADER_PARAMETER(uint32, SensorCount)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("CLEAR_RESULTS_CS"), 1);
		OutEnvironment.SetDefine(TEXT("CLEAR_RESULTS_CHUNK_SIZE"), ChunkSize);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridClearResultsCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "ClearNearestSensors", SF_Compute);

class FACFSensorGridBvhPrimeBoundsCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridBvhPrimeBoundsCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridBvhPrimeBoundsCs, FGlobalShader);

public:
	static const uint32 ChunkSize = 128;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SensorLocations)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, PartialBoundingBoxes)
		SHADER_PARAMETER(uint32, SensorCount)
		SHADER_PARAMETER(uint32, PaddedIntermediateCount)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("PRIME_BOUNDS_GENERATION_CS"), 1);
		OutEnvironment.SetDefine(TEXT("PRIME_BOUNDS_CHUNK_SIZE"), ChunkSize);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridBvhPrimeBoundsCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "PrimeBounds", SF_Compute);

class FACFSensorGridBvhFinalizeBoundsCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridBvhFinalizeBoundsCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridBvhFinalizeBoundsCs, FGlobalShader);

public:
	static const uint32 ChunkSize = 128;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SourceBoundingBoxes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, TargetBoundingBoxes)
		SHADER_PARAMETER(uint32, SourceBoundsCount)
		SHADER_PARAMETER(uint32, PaddedSourceBoundsCount)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("FINALIZE_BOUNDS_CS"), 1);
		OutEnvironment.SetDefine(TEXT("FINALIZE_BOUNDS_CHUNK_SIZE"), ChunkSize);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridBvhFinalizeBoundsCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "FinalizeBounds", SF_Compute);


class FACFSensorGridBvhMortonCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridBvhMortonCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridBvhMortonCs, FGlobalShader);

public:
	static const uint32 ChunkSize = 128;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SensorLocations)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, BoundingBoxes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, LeafIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, MortonCodes)
		SHADER_PARAMETER(uint32, SensorCount)
		SHADER_PARAMETER(uint32, PaddedOutputCount)
		SHADER_PARAMETER(uint32, OwnerBitCount)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("MORTON_GENERATTION_CS"), 1);
		OutEnvironment.SetDefine(TEXT("MORTON_GENERATION_CHUNK_SIZE"), ChunkSize);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridBvhMortonCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "MortonGeneration", SF_Compute);

class FACFSensorGridMortonCompactionCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridMortonCompactionCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridMortonCompactionCs, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InputValues)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutputValues)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, DuplicateCounts)
		SHADER_PARAMETER(uint32, ValueCount)
		SHADER_PARAMETER(uint32, OwnerBitCount)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("MORTON_COMPACTION_CS"), 1);
		OutEnvironment.SetDefine(TEXT("MORTON_COMPACTION_CHUNK_SIZE"), ACFSensorGridShaders::MortonCompactionBufferSize);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridMortonCompactionCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "MortonCompaction", SF_Compute);

class FACFSensorGridBuildCopyCommandsCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridBuildCopyCommandsCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridBuildCopyCommandsCs, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, DuplicateCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CompactedValues)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint4>, CopyCommands)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, ElementsPerOwner)
		SHADER_PARAMETER(uint32, OwnerCount)
		SHADER_PARAMETER(uint32, GroupsPerOwner)
		SHADER_PARAMETER(uint32, MaxElementsPerGroup)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("MORTON_BUILD_COPY_COMMANDS_CS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridBuildCopyCommandsCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "BuildCopyCommands", SF_Compute);

class FACFSensorGridShuffleDataCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridShuffleDataCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridShuffleDataCs, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InputValues)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint4>, CopyCommands)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutputValues)
		SHADER_PARAMETER(uint32, ValueCount)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("MORTON_SHUFFLE_DATA_CS"), 1);
		OutEnvironment.SetDefine(TEXT("MORTON_SHUFFLE_CHUNK_SIZE"), ACFSensorGridShaders::MortonCompactionBufferSize);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridShuffleDataCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "MortonShuffleData", SF_Compute);

class FACFSensorGridBvhGenTopDownCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridBvhGenTopDownCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridBvhGenTopDownCs, FGlobalShader);

public:
	static const uint32 ChunkSize = 128;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, LeafCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, LeafIndices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, MortonCodes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FInternalNode>, InternalNodes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, ParentIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, AccumulationGates)
		SHADER_PARAMETER(uint32, InternalNodeParentOffset)
		SHADER_PARAMETER(uint32, PaddedLeafNodeCount)
		SHADER_PARAMETER(uint32, PaddedInternalNodeCount)
		SHADER_PARAMETER(uint32, PaddedParentCount)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("HIERARCHY_GENERATION_TOP_DOWN_CS"), 1);
		OutEnvironment.SetDefine(TEXT("HIERARCHY_GENERATION_CHUNK_SIZE"), ChunkSize);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridBvhGenTopDownCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "HierarchyGeneration_TopDown", SF_Compute);

class FACFSensorGridBvhGenBottomUpCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridBvhGenBottomUpCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridBvhGenBottomUpCs, FGlobalShader);

public:
	static const uint32 ChunkSize = 128;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, SensorCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SensorLocations)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ParentIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FInternalNode>, InternalNodes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, AccumulationGates)
		SHADER_PARAMETER(uint32, InternalNodeParentOffset)
		SHADER_PARAMETER(uint32, MaxSensorsPerOwner)
		SHADER_PARAMETER(uint32, PaddedInternalNodeCount)
		SHADER_PARAMETER(uint32, PaddedParentCount)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("BOUNDS_GENERATION_BOTTOM_UP_CS"), 1);
		OutEnvironment.SetDefine(TEXT("BOUNDS_GENERATION_CHUNK_SIZE"), ChunkSize);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridBvhGenBottomUpCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "BoundsGeneration_BottomUp", SF_Compute);

class FACFSensorGridBvhTraversalCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FACFSensorGridBvhTraversalCs);
	SHADER_USE_PARAMETER_STRUCT(FACFSensorGridBvhTraversalCs, FGlobalShader);

public:
	static const uint32 ChunkSize = 32;
	static const uint32 SlackSize = 4;
	static const uint32 MinSensorCountLogTwo = 12;
	static const uint32 MaxSensorCountLogTwo = 14;

	class FMaxSensorCountLogTwo : SHADER_PERMUTATION_RANGE_INT("MAX_SENSOR_COUNT_LOG_TWO", MinSensorCountLogTwo, MaxSensorCountLogTwo - MinSensorCountLogTwo + 1);
	using FPermutationDomain = TShaderPermutationDomain<FMaxSensorCountLogTwo>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ASCENTSENSORGRIDSHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, SensorCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SensorLocations)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FInternalNodes>, InternalNodes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSensorInfo>, NearestSensors)
		SHADER_PARAMETER(float, MaxDistance)
		SHADER_PARAMETER(uint32, MaxSensorsPerOwner)
		SHADER_PARAMETER(uint32, PaddedSensorCount)
		SHADER_PARAMETER(uint32, PaddedInternalNodeCount)
		SHADER_PARAMETER(uint32, OwnerCount)
		SHADER_PARAMETER(uint32, SensorGridFactor)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("BVH_TRAVERSAL_CS"), 1);
		OutEnvironment.SetDefine(TEXT("BVH_TRAVERSAL_CHUNK_SIZE"), ChunkSize);
		OutEnvironment.SetDefine(TEXT("BVH_TRAVERSAL_STACK_SLACK_SIZE"), SlackSize);
	}
};
IMPLEMENT_GLOBAL_SHADER(FACFSensorGridBvhTraversalCs, "/ACFSensorGrid/ACFSensorGridBvh.usf", "BvhTraversal", SF_Compute);

BEGIN_SHADER_PARAMETER_STRUCT(FACFSensorGridSortPassParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV_ARRAY(Buffer<uint>, MortonCodeSRVs, [2])
	SHADER_PARAMETER_RDG_BUFFER_UAV_ARRAY(RWBuffer<uint>, MortonCodeUAVs, [2])
	SHADER_PARAMETER_RDG_BUFFER_SRV_ARRAY(Buffer<uint>, LeafIndicesSRVs, [2])
	SHADER_PARAMETER_RDG_BUFFER_UAV_ARRAY(RWBuffer<uint>, LeafIndicesUAVs, [2])
END_SHADER_PARAMETER_STRUCT()

uint32 FACFSensorGridHelper::GetMaxSensorDensity()
{
	return 1 << (FACFSensorGridBvhTraversalCs::MaxSensorCountLogTwo / 2);
}

uint32 FACFSensorGridHelper::GetMaxOwnerCount()
{
	return (1 << ACFSensorGridShaders::MortonCodeBitsReservedForOwner) - 1;
}

FACFSensorGridHelper::FACFSensorGridHelper(ERHIFeatureLevel::Type InFeatureLevel, const FUintVector4& InSensorGridDimensions, uint32 FrameIndex)
	: FeatureLevel(InFeatureLevel)
	, SensorGridDimensions(InSensorGridDimensions)
{
}

FACFSensorGridHelper::~FACFSensorGridHelper()
{
}

bool FACFSensorGridHelper::FTransientResources::Supports(const FResourceSizingInfo& OtherSizingInfo) const
{
	return OtherSizingInfo.SensorCount <= SizingInfo.SensorCount && OtherSizingInfo.OwnerCount <= SizingInfo.OwnerCount;
}

void FACFSensorGridHelper::FTransientResources::Reset()
{
	SensorLocations = nullptr;
	UserChannelData = nullptr;
	PartialBounds = nullptr;
	LeafIndices[0] = nullptr;
	LeafIndices[1] = nullptr;
	MortonCodes[0] = nullptr;
	MortonCodes[1] = nullptr;
	DuplicateCounts = nullptr;
	CopyCommands = nullptr;
	ParentIndices = nullptr;
	HierarchyGates = nullptr;
	OwnerBoundingBoxes = nullptr;
	InternalNodes = nullptr;
	SensorCounts = nullptr;

	HasBuffers = false;
}

void FACFSensorGridHelper::FTransientResources::Build(FRDGBuilder& GraphBuilder, const FResourceSizingInfo& InSizingInfo)
{
	SizingInfo = InSizingInfo;

	if (SizingInfo.SensorCount <= 1)
	{
		Reset();
		return;
	}

	const uint32 InternalNodesPerOwner = SizingInfo.SensorCount - 1;
	const uint32 ParentsPerOwner = SizingInfo.SensorCount + InternalNodesPerOwner;
	const uint32 AlignedSensorsPerOwner = Align(InSizingInfo.SensorCount, ACFSensorGridShaders::SensorsPerOwnerAlignment);
	const uint32 DuplicateCountBlocksPerOwner = FMath::DivideAndRoundUp(AlignedSensorsPerOwner, ACFSensorGridShaders::MortonCompactionBufferSize);
	const uint32 DuplicateCountBlocks = DuplicateCountBlocksPerOwner * SizingInfo.OwnerCount;
	const uint32 IntermediaryBoundsPerOwner = FMath::DivideAndRoundUp(InSizingInfo.SensorCount, FACFSensorGridBvhPrimeBoundsCs::ChunkSize);

	SensorLocations = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), SizingInfo.SensorCount * SizingInfo.OwnerCount), TEXT("ACFSensorGridSensorLocations"));

	if (SizingInfo.UserChannelCount > 0)
	{
		UserChannelData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), SizingInfo.SensorCount * SizingInfo.OwnerCount * SizingInfo.UserChannelCount), TEXT("ACFSensorGridUserChannelData"));
	}

	PartialBounds = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), Align(IntermediaryBoundsPerOwner, FACFSensorGridBvhFinalizeBoundsCs::ChunkSize) * SizingInfo.OwnerCount * 2), TEXT("ACFSensorGridBvhPartialBounds"));

	for (FRDGBufferRef& LeafIndicesBuff : LeafIndices)
	{
		LeafIndicesBuff = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), AlignedSensorsPerOwner * SizingInfo.OwnerCount), TEXT("ACFSensorGridBvhLeafIndicesSorting"));
	}

	for (FRDGBufferRef& MortonCodesBuff : MortonCodes)
	{
		MortonCodesBuff = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), AlignedSensorsPerOwner * SizingInfo.OwnerCount), TEXT("ACFSensorGridBvhMortonCodesSorting"));
	}

	DuplicateCounts = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), DuplicateCountBlocks), TEXT("ACFSensorGridBvhDuplicateCounts"));

	CopyCommands = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FUintVector4), DuplicateCountBlocks), TEXT("ACFSensorGridBvhCopyCommands"));

	ParentIndices = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), Align(ParentsPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment) * SizingInfo.OwnerCount), TEXT("ACFSensorGridBvhParentIndices"));

	HierarchyGates = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), Align(InternalNodesPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment) * SizingInfo.OwnerCount), TEXT("ACFSensorGridBvhAccumGates"));

	// buffer to store the bounds for each of the owners
	OwnerBoundingBoxes = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), SizingInfo.OwnerCount * 2), TEXT("ACFSensorGridBvhOwnerBounds"));

	InternalNodes = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(ACFSensorGridShaders::FInternalNode), Align(InternalNodesPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment) * SizingInfo.OwnerCount), TEXT("ACFSensorGridBvhInternalNodes"));

	SensorCounts = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), SizingInfo.OwnerCount), TEXT("ACFSensorGridBvhSensorCounts"));

	TraversalResults = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FSensorInfo), SizingInfo.SensorCount * SizingInfo.OwnerCount), TEXT("ACFSensorGridTraversalResults"));

	HasBuffers = true;
}

void FACFSensorGridHelper::InitBuffers(
	FRDGBuilder& GraphBuilder,
	FTransientResources& TransientResources)
{
	check(TransientResources.HasBuffers);
	FACFSensorGridResetSensorLocationsCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridResetSensorLocationsCs::FParameters>();
	PassParameters->SensorsToReset = GraphBuilder.CreateUAV(TransientResources.SensorLocations, PF_A32B32G32R32F);
	PassParameters->SensorCount = (1 << SensorGridDimensions.X) * (1 << SensorGridDimensions.Y) * SensorGridDimensions.Z;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ACFSensorGrid_InitBuffers"),
		TShaderMapRef<FACFSensorGridResetSensorLocationsCs>(GetGlobalShaderMap(FeatureLevel)),
		PassParameters,
		FIntVector(FMath::DivideAndRoundUp(PassParameters->SensorCount, FACFSensorGridResetSensorLocationsCs::ChunkSize), 1, 1));

	if (TransientResources.SizingInfo.UserChannelCount > 0)
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TransientResources.UserChannelData, PF_A32B32G32R32F), 0);
	}
}

void FACFSensorGridHelper::ResetResults(
	FRDGBuilder& GraphBuilder,
	FRDGBufferUAVRef ResultsUav)
{
	FACFSensorGridClearResultsCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridClearResultsCs::FParameters>();
	PassParameters->NearestSensors = ResultsUav;
	PassParameters->SensorCount = (1 << SensorGridDimensions.X) * (1 << SensorGridDimensions.Y) * SensorGridDimensions.Z;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ACFSensorGrid_ClearResults"),
		TShaderMapRef<FACFSensorGridClearResultsCs>(GetGlobalShaderMap(FeatureLevel)),
		PassParameters,
		FIntVector(FMath::DivideAndRoundUp(PassParameters->SensorCount, FACFSensorGridResetSensorLocationsCs::ChunkSize), 1, 1));
}

void FACFSensorGridHelper::GenerateOwnerBounds(
	FRDGBuilder& GraphBuilder,
	FTransientResources& TransientResources,
	FRDGBufferSRVRef SensorLocationsSrv)
{
	check(TransientResources.HasBuffers);
	RDG_EVENT_SCOPE(GraphBuilder, "GenerateOwnerBounds");

	const uint32 SensorsPerOwner = (1 << SensorGridDimensions.X) * (1 << SensorGridDimensions.Y);
	const uint32 IntermediaryBoundsPerOwner = FMath::DivideAndRoundUp(SensorsPerOwner, FACFSensorGridBvhPrimeBoundsCs::ChunkSize);

	// PrimeBounds
	{
		FACFSensorGridBvhPrimeBoundsCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridBvhPrimeBoundsCs::FParameters>();
		PassParameters->SensorLocations = SensorLocationsSrv;
		PassParameters->PartialBoundingBoxes = GraphBuilder.CreateUAV(TransientResources.PartialBounds, PF_A32B32G32R32F);
		PassParameters->SensorCount = SensorsPerOwner;
		PassParameters->PaddedIntermediateCount = Align(IntermediaryBoundsPerOwner, FACFSensorGridBvhFinalizeBoundsCs::ChunkSize);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ACFSensorGrid_PrimetLeafBounds"),
			TShaderMapRef<FACFSensorGridBvhPrimeBoundsCs>(GetGlobalShaderMap(FeatureLevel)),
			PassParameters,
			FIntVector(IntermediaryBoundsPerOwner, SensorGridDimensions.Z, 1));
	}

	// FinalizeBounds
	{
		FACFSensorGridBvhFinalizeBoundsCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridBvhFinalizeBoundsCs::FParameters>();
		PassParameters->SourceBoundingBoxes = GraphBuilder.CreateSRV(TransientResources.PartialBounds, PF_A32B32G32R32F);
		PassParameters->TargetBoundingBoxes = GraphBuilder.CreateUAV(TransientResources.OwnerBoundingBoxes, PF_A32B32G32R32F);
		PassParameters->SourceBoundsCount = IntermediaryBoundsPerOwner;
		PassParameters->PaddedSourceBoundsCount = Align(IntermediaryBoundsPerOwner, FACFSensorGridBvhFinalizeBoundsCs::ChunkSize);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ACFSensorGrid_FinalizeLeafBounds"),
			TShaderMapRef<FACFSensorGridBvhFinalizeBoundsCs>(GetGlobalShaderMap(FeatureLevel)),
			PassParameters,
			FIntVector(SensorGridDimensions.Z, 1, 1));
	}
}

void FACFSensorGridHelper::GenerateSortedLeaves(
	FRDGBuilder& GraphBuilder,
	FTransientResources& TransientResources,
	FRDGBufferSRVRef SensorLocationsSrv)
{
	check(TransientResources.HasBuffers);
	RDG_EVENT_SCOPE(GraphBuilder, "GenerateSortedLeaves");

	const uint32 SensorsPerOwner = (1 << SensorGridDimensions.X) * (1 << SensorGridDimensions.Y);
	const uint32 InternalNodesPerOwner = SensorsPerOwner - 1;
	const uint32 AlignedSensorsPerOwner = Align(SensorsPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment);

	// for compaction we'll collapse each set of sensors per owner
	check((ACFSensorGridShaders::SensorsPerOwnerAlignment % ACFSensorGridShaders::MortonCompactionBufferSize) == 0);

	const uint32 DuplicateCountBlocksPerOwner = FMath::DivideAndRoundUp(AlignedSensorsPerOwner, ACFSensorGridShaders::MortonCompactionBufferSize);
	const uint32 DuplicateCountBlocks = DuplicateCountBlocksPerOwner * SensorGridDimensions.Z;

	// Generate morton codes
	{
		FACFSensorGridBvhMortonCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridBvhMortonCs::FParameters>();
		PassParameters->SensorLocations = SensorLocationsSrv;
		PassParameters->BoundingBoxes = GraphBuilder.CreateSRV(TransientResources.OwnerBoundingBoxes, PF_A32B32G32R32F);
		PassParameters->LeafIndices = GraphBuilder.CreateUAV(TransientResources.LeafIndices[1], PF_R32_UINT);
		PassParameters->MortonCodes = GraphBuilder.CreateUAV(TransientResources.MortonCodes[0], PF_R32_UINT);
		PassParameters->SensorCount = SensorsPerOwner;
		PassParameters->PaddedOutputCount = Align(SensorsPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment);
		PassParameters->OwnerBitCount = ACFSensorGridShaders::MortonCodeBitsReservedForOwner;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ACFSensorGrid_BvhMorton"),
			TShaderMapRef<FACFSensorGridBvhMortonCs>(GetGlobalShaderMap(FeatureLevel)),
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(SensorsPerOwner, FACFSensorGridBvhMortonCs::ChunkSize), SensorGridDimensions.Z, 1));
	}

	// GPUSort!
	{
		FACFSensorGridSortPassParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridSortPassParameters>();
		PassParameters->MortonCodeSRVs[0] = GraphBuilder.CreateSRV(TransientResources.MortonCodes[0], PF_R32_UINT);
		PassParameters->MortonCodeSRVs[1] = GraphBuilder.CreateSRV(TransientResources.MortonCodes[1], PF_R32_UINT);
		PassParameters->MortonCodeUAVs[0] = GraphBuilder.CreateUAV(TransientResources.MortonCodes[0], PF_R32_UINT);
		PassParameters->MortonCodeUAVs[1] = GraphBuilder.CreateUAV(TransientResources.MortonCodes[1], PF_R32_UINT);

		PassParameters->LeafIndicesSRVs[0] = GraphBuilder.CreateSRV(TransientResources.LeafIndices[0], PF_R32_UINT);
		PassParameters->LeafIndicesSRVs[1] = GraphBuilder.CreateSRV(TransientResources.LeafIndices[1], PF_R32_UINT);
		PassParameters->LeafIndicesUAVs[0] = GraphBuilder.CreateUAV(TransientResources.LeafIndices[0], PF_R32_UINT);
		PassParameters->LeafIndicesUAVs[1] = GraphBuilder.CreateUAV(TransientResources.LeafIndices[1], PF_R32_UINT);
		
		const int32 NumTotalSensors = SensorsPerOwner * SensorGridDimensions.Z;

		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TransientResources.MortonCodes[1], PF_R32_UINT), 0);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TransientResources.LeafIndices[0], PF_R32_UINT), 0);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ACFSensorGrid_SortMortonCodes"),
			PassParameters,
			ERDGPassFlags::Compute,
			[
				PassParameters,
				NumTotalSensors,
				FeatureLevel = FeatureLevel
			](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				FGPUSortBuffers SortBuffers;
				SortBuffers.RemoteKeySRVs[0] = PassParameters->MortonCodeSRVs[0]->GetRHI();
				SortBuffers.RemoteKeySRVs[1] = PassParameters->MortonCodeSRVs[1]->GetRHI();
				SortBuffers.RemoteKeyUAVs[0] = PassParameters->MortonCodeUAVs[0]->GetRHI();
				SortBuffers.RemoteKeyUAVs[1] = PassParameters->MortonCodeUAVs[1]->GetRHI();

				SortBuffers.RemoteValueSRVs[0] = PassParameters->LeafIndicesSRVs[1]->GetRHI();
				SortBuffers.RemoteValueSRVs[1] = PassParameters->LeafIndicesSRVs[0]->GetRHI();
				SortBuffers.RemoteValueUAVs[0] = PassParameters->LeafIndicesUAVs[1]->GetRHI();
				SortBuffers.RemoteValueUAVs[1] = PassParameters->LeafIndicesUAVs[0]->GetRHI();

				const int32 BufferIndex = 0;
				const uint32 KeyMask = 0xFFFFFFFF;

				int32 ResultBufferIndex = SortGPUBuffers(RHICmdList, SortBuffers, BufferIndex, KeyMask, NumTotalSensors, FeatureLevel);
				check(ResultBufferIndex == 0);
			}
		);
	}

	// go through the sorted buffer and find (and get rid of) duplicates
	{
		FACFSensorGridMortonCompactionCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridMortonCompactionCs::FParameters>();
		PassParameters->InputValues = GraphBuilder.CreateSRV(TransientResources.MortonCodes[0], PF_R32_UINT);
		PassParameters->OutputValues = GraphBuilder.CreateUAV(TransientResources.MortonCodes[1], PF_R32_UINT);
		PassParameters->DuplicateCounts = GraphBuilder.CreateUAV(TransientResources.DuplicateCounts, PF_R32_UINT);
		PassParameters->ValueCount = SensorsPerOwner * SensorGridDimensions.Z;
		PassParameters->OwnerBitCount = ACFSensorGridShaders::MortonCodeBitsReservedForOwner;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ACFSensorGrid_MortonCompaction"),
			TShaderMapRef<FACFSensorGridMortonCompactionCs>(GetGlobalShaderMap(FeatureLevel)),
			PassParameters,
			FIntVector(DuplicateCountBlocks, 1, 1));
	}

	{
		FACFSensorGridBuildCopyCommandsCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridBuildCopyCommandsCs::FParameters>();
		PassParameters->DuplicateCounts = GraphBuilder.CreateSRV(TransientResources.DuplicateCounts, PF_R32_UINT);
		PassParameters->CompactedValues = GraphBuilder.CreateSRV(TransientResources.MortonCodes[1], PF_R32_UINT);
		PassParameters->CopyCommands = GraphBuilder.CreateUAV(TransientResources.CopyCommands, PF_R32G32B32A32_UINT);
		PassParameters->ElementsPerOwner = GraphBuilder.CreateUAV(TransientResources.SensorCounts, PF_R32_UINT);
		PassParameters->OwnerCount = SensorGridDimensions.Z;
		PassParameters->GroupsPerOwner = DuplicateCountBlocksPerOwner;
		PassParameters->MaxElementsPerGroup = ACFSensorGridShaders::MortonCompactionBufferSize;


		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ACFSensorGrid_BuildCopyCommands"),
			TShaderMapRef<FACFSensorGridBuildCopyCommandsCs>(GetGlobalShaderMap(FeatureLevel)),
			PassParameters,
			FIntVector(SensorGridDimensions.Z, 1, 1));
	}

	{
		TShaderMapRef<FACFSensorGridShuffleDataCs> ShuffleDataShader(GetGlobalShaderMap(FeatureLevel));

		FACFSensorGridShuffleDataCs::FParameters* ShuffleMortonParameters = GraphBuilder.AllocParameters<FACFSensorGridShuffleDataCs::FParameters>();
		ShuffleMortonParameters->InputValues = GraphBuilder.CreateSRV(TransientResources.MortonCodes[1], PF_R32_UINT);
		ShuffleMortonParameters->CopyCommands = GraphBuilder.CreateSRV(TransientResources.CopyCommands, PF_R32G32B32A32_UINT);
		ShuffleMortonParameters->OutputValues = GraphBuilder.CreateUAV(TransientResources.MortonCodes[0], PF_R32_UINT);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ACFSensorGrid_ShuffleMorton"),
			ShuffleDataShader,
			ShuffleMortonParameters,
			FIntVector(DuplicateCountBlocks, 1, 1));

		FACFSensorGridShuffleDataCs::FParameters* ShuffleIndicesParameters = GraphBuilder.AllocParameters<FACFSensorGridShuffleDataCs::FParameters>();
		ShuffleIndicesParameters->InputValues = GraphBuilder.CreateSRV(TransientResources.LeafIndices[1], PF_R32_UINT);
		ShuffleIndicesParameters->CopyCommands = GraphBuilder.CreateSRV(TransientResources.CopyCommands, PF_R32G32B32A32_UINT);
		ShuffleIndicesParameters->OutputValues = GraphBuilder.CreateUAV(TransientResources.LeafIndices[0], PF_R32_UINT);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ACFSensorGrid_ShuffleIndices"),
			ShuffleDataShader,
			ShuffleIndicesParameters,
			FIntVector(DuplicateCountBlocks, 1, 1));
	}
}

void FACFSensorGridHelper::GenerateBvh(
	FRDGBuilder& GraphBuilder,
	FTransientResources& TransientResources,
	FRDGBufferSRVRef SensorLocationsSrv)
{
	check(TransientResources.HasBuffers);
	RDG_EVENT_SCOPE(GraphBuilder, "GenerateBvh");

	const uint32 SensorsPerOwner = (1 << SensorGridDimensions.X) * (1 << SensorGridDimensions.Y);
	const uint32 InternalNodesPerOwner = SensorsPerOwner - 1;
	const uint32 ParentsPerOwner = SensorsPerOwner + InternalNodesPerOwner;

	// top down pass for generating node relationships
	{
		check((ACFSensorGridShaders::SensorsPerOwnerAlignment % FACFSensorGridBvhGenTopDownCs::ChunkSize) == 0);

		FACFSensorGridBvhGenTopDownCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridBvhGenTopDownCs::FParameters>();
		PassParameters->LeafCounts = GraphBuilder.CreateSRV(TransientResources.SensorCounts, PF_R32_UINT);
		PassParameters->LeafIndices = GraphBuilder.CreateSRV(TransientResources.LeafIndices[0], PF_R32_UINT);
		PassParameters->MortonCodes = GraphBuilder.CreateSRV(TransientResources.MortonCodes[0], PF_R32_UINT);
		PassParameters->InternalNodes = GraphBuilder.CreateUAV(TransientResources.InternalNodes, PF_Unknown);
		PassParameters->ParentIndices = GraphBuilder.CreateUAV(TransientResources.ParentIndices, PF_R32_UINT);
		PassParameters->AccumulationGates = GraphBuilder.CreateUAV(TransientResources.HierarchyGates, PF_R32_UINT);
		PassParameters->InternalNodeParentOffset = SensorsPerOwner;
		PassParameters->PaddedLeafNodeCount = Align(SensorsPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment);
		PassParameters->PaddedInternalNodeCount = Align(InternalNodesPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment);
		PassParameters->PaddedParentCount = Align(ParentsPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ACFSensorGrid_BvhGenTopDown"),
			TShaderMapRef<FACFSensorGridBvhGenTopDownCs>(GetGlobalShaderMap(FeatureLevel)),
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(InternalNodesPerOwner, FACFSensorGridBvhGenTopDownCs::ChunkSize), SensorGridDimensions.Z, 1));
	}

	// bottom up pass for completing bounds generation
	{
		check((ACFSensorGridShaders::SensorsPerOwnerAlignment % FACFSensorGridBvhGenBottomUpCs::ChunkSize) == 0);

		FACFSensorGridBvhGenBottomUpCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridBvhGenBottomUpCs::FParameters>();
		PassParameters->SensorCounts = GraphBuilder.CreateSRV(TransientResources.SensorCounts, PF_R32_UINT);
		PassParameters->SensorLocations = SensorLocationsSrv;
		PassParameters->ParentIndices = GraphBuilder.CreateSRV(TransientResources.ParentIndices, PF_R32_UINT);
		PassParameters->InternalNodes = GraphBuilder.CreateUAV(TransientResources.InternalNodes, PF_Unknown);
		PassParameters->AccumulationGates = GraphBuilder.CreateUAV(TransientResources.HierarchyGates, PF_R32_UINT);
		PassParameters->InternalNodeParentOffset = SensorsPerOwner;
		PassParameters->MaxSensorsPerOwner = SensorsPerOwner;
		PassParameters->PaddedInternalNodeCount = Align(InternalNodesPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment);
		PassParameters->PaddedParentCount = Align(ParentsPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ACFSensorGrid_BvhGenBottomUp"),
			TShaderMapRef<FACFSensorGridBvhGenBottomUpCs>(GetGlobalShaderMap(FeatureLevel)),
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(SensorsPerOwner, FACFSensorGridBvhGenBottomUpCs::ChunkSize), SensorGridDimensions.Z, 1));
	}
}

void FACFSensorGridHelper::RunTraversals(
	FRDGBuilder& GraphBuilder,
	const FVector2D& GlobalSensorRange,
	FTransientResources& TransientResources,
	FRDGBufferSRVRef SensorLocationsSrv,
	FRDGBufferUAVRef ResultsUav)
{
	check(TransientResources.HasBuffers);

	{
		const uint32 SensorsPerOwner = (1 << SensorGridDimensions.X) * (1 << SensorGridDimensions.Y);
		const uint32 SensorsPerOwnerLogTwo = FMath::Max(FACFSensorGridBvhTraversalCs::MinSensorCountLogTwo, FMath::CeilLogTwo(SensorsPerOwner));
		const uint32 InternalNodesPerOwner = SensorsPerOwner - 1;

		if (!ensure(SensorsPerOwnerLogTwo <= FACFSensorGridBvhTraversalCs::MaxSensorCountLogTwo))
		{
			return;
		}

		FACFSensorGridBvhTraversalCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FACFSensorGridBvhTraversalCs::FParameters>();
		PassParameters->SensorCounts = GraphBuilder.CreateSRV(TransientResources.SensorCounts, PF_R32_UINT);
		PassParameters->SensorLocations = SensorLocationsSrv;
		PassParameters->InternalNodes = GraphBuilder.CreateSRV(TransientResources.InternalNodes);
		PassParameters->NearestSensors = ResultsUav;
		PassParameters->MaxDistance = GlobalSensorRange.Y;
		PassParameters->MaxSensorsPerOwner = SensorsPerOwner;
		PassParameters->PaddedSensorCount = Align(SensorsPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment);
		PassParameters->PaddedInternalNodeCount = Align(InternalNodesPerOwner, ACFSensorGridShaders::SensorsPerOwnerAlignment);
		PassParameters->OwnerCount = SensorGridDimensions.Z;
		PassParameters->SensorGridFactor = SensorGridDimensions.X;

		FACFSensorGridBvhTraversalCs::FPermutationDomain PermutationVector;
		PermutationVector.Set<FACFSensorGridBvhTraversalCs::FMaxSensorCountLogTwo>(SensorsPerOwnerLogTwo);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ACFSensorGrid_BvhTraversal"),
			TShaderMapRef<FACFSensorGridBvhTraversalCs>(GetGlobalShaderMap(FeatureLevel), PermutationVector),
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(SensorsPerOwner, FACFSensorGridBvhTraversalCs::ChunkSize), SensorGridDimensions.Z, 1));
	}
}

void FACFSensorGridHelper::NearestSensors(
	FRDGBuilder& GraphBuilder,
	const FVector2D& GlobalSensorRange,
	FTransientResources& TransientResources)
{
	RDG_EVENT_SCOPE(GraphBuilder, "ACFSensorGrid_NearestSensors");

	FRDGBufferSRVRef LocationsSRV = GraphBuilder.CreateSRV(TransientResources.SensorLocations, PF_A32B32G32R32F);
	FRDGBufferUAVRef ResultsUAV = GraphBuilder.CreateUAV(TransientResources.TraversalResults, PF_Unknown);

	ResetResults(GraphBuilder, ResultsUAV);

	if (TransientResources.SizingInfo.OwnerCount > 1 && TransientResources.HasBuffers && !GACFSensorGridBuildDisabled)
	{
		GenerateOwnerBounds(GraphBuilder, TransientResources, LocationsSRV);
		GenerateSortedLeaves(GraphBuilder, TransientResources, LocationsSRV);
		GenerateBvh(GraphBuilder, TransientResources, LocationsSRV);
		RunTraversals(GraphBuilder, GlobalSensorRange, TransientResources, LocationsSRV, ResultsUAV);
	}
}
