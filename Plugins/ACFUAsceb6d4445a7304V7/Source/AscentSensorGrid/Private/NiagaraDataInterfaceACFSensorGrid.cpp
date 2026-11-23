// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceACFSensorGrid.h"

#include "ACFSensorGridHelper.h"
#include "GlobalDistanceFieldParameters.h"
#include "NiagaraCompileHashVisitor.h"
#include "NiagaraComponent.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraRenderGraphUtils.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraTypes.h"
#include "NiagaraShader.h"
#include "NiagaraWorldManager.h"
#include "RenderResource.h"
#include "Shader.h"
#include "ShaderCore.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterUtils.h"

static float GACFSensorGridRadiusOverride = -1.0f;
static FAutoConsoleVariableRef CVarACFSensorGridRadiusOverride(
	TEXT("acf.sensorgrid.RadiusOverride"),
	GACFSensorGridRadiusOverride,
	TEXT("When > 0, this will override any ACFSensorGrid DI settings"),
	ECVF_Default
);

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceACFSensorGrid"

namespace NDIACFSensorGridLocal
{
	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, RWSensorLocations)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FSensorInfo>, SensorInfo)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, RWSensorGridUserChannelData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SensorGridPreviousUserChannelData)

		SHADER_PARAMETER(FUintVector4, ReadSensorGridDimensions)
		SHADER_PARAMETER(FUintVector4, WriteSensorGridDimensions)
		SHADER_PARAMETER(int32, SensorGridWriteIndex)
		SHADER_PARAMETER(int32, SensorGridReadIndex)
		SHADER_PARAMETER(int32, SensorGridUserChannelCount)
	END_SHADER_PARAMETER_STRUCT()

	static const TCHAR* CommonShaderFile = TEXT("/ACFSensorGrid/NiagaraDataInterfaceACFSensorGrid.ush");
	static const TCHAR* TemplateShaderFile = TEXT("/ACFSensorGrid/NiagaraDataInterfaceACFSensorGridTemplate.ush");

	// Serialized into the graph of every Niagara script calling this Data Interface. Renaming it orphans
	// the Data Interface pin on already authored assets, so it has to keep its original name.
	static const FName DataInterfacePinName(TEXT("CitySampleSensorGrid"));

	static const FName UpdateSensorName(TEXT("UpdateSensorGpu"));
	static const FText UpdateSensorDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("UpdateSensorDescription", "Updates the sensor location, should be called each frame"), FText());

	static const FName FindNearestName(TEXT("FindNearestGpu"));
	static const FText FindNearestDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("FindNearestDescription", "Grabs information on the closest sensor (from a different grid) from the previous frame"), FText());

	static const FName ReadUserChannelName(TEXT("ReadSensorUserChannel"));
	static const FText ReadUserChannelDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("ReadUserChannelDescription", "Reads a user data channel from the previous frame"), FText());

	static const FName WriteUserChannelName(TEXT("WriteSensorUserChannel"));
	static const FText WriteUserChannelDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("WriteUserChannelDescription", "Sets a user data channel, should be called each frame"), FText());

	enum NodeVersion : int32
	{
		NodeVersion_Initial = 0,
		Nodeversion_Reworked_Output,
		NodeVersion_Readded_SensorOutput,

		VersionPlusOne,
		NodeVersion_Latest = VersionPlusOne - 1,
	};

	static const FText SensorXDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("SensorXDescription", "X coordinate of the sensor"), FText());
	static const FText SensorYDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("SensorYDescription", "Y coordinate of the sensor"), FText());
	static const FText OwnerIndexDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("OwnerIndexDescription", "Identifier of the grid the sensor belongs to"), FText());
	static const FText LocationDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("LocationDescription", "Position in 3D space"), FText());
	static const FText SensorRangeDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("SensorRangeDescription", "Experimental - Per sensor radius (additive to the global search radius)"), FText());
	static const FText SensorValidDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("SensorValidDescription", "Indicates that the sensor is findable from other grids"), FText());
	static const FText UserChannelIndexDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("UserChannelIndexDescription", "Index of the user channel"), FText());
	static const FText UserChannelValueDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("UserChannelValueDescription", "Value of the user channel"), FText());

	static const FText OutputLocationDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("OutputLocationDescription", "Location of the closest sensor from the previous frame"), FText());
	static const FText OutputDistanceDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("OutputDistanceDescription", "Distance to the closest sensor from the previous frame"), FText());
	static const FText OutputIsValidDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("OutputIsValidDescription", "Indicates whether a valid sensor was found the previous frame"), FText());
	static const FText OutputSensorXDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("OutputSensorXDescription", "X coordinate of the closest sensor from the previous frame"), FText());
	static const FText OutputSensorYDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("OutputSensorYdDescription", "Y coordinate of the closest sensor from the previous frame"), FText());
	static const FText OutputOwnerIndexDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("OutputOwnerIndexDescription", "Identifier of the grid associatd with the closest sensor from the previous frame - not guaranteed to be stable"), FText());
	static const FText OutputUserChannelDescription = IF_WITH_EDITORONLY_DATA(LOCTEXT("OutputUserChanneldDescription", "Value of the user channel from the previous frame"), FText());

	struct FSensorGridNetworkProxy
	{
		TRefCountPtr<FRDGPooledBuffer> ExtractedSensorInfo;
		TRefCountPtr<FRDGPooledBuffer> ExtractedUserChannelData;

		TUniquePtr<FACFSensorGridHelper::FTransientResources> TransientResources;

		// mapping between the system instance ID and the subnetwork within the buffers
		TMap<FNiagaraSystemInstanceID, int32> InstanceOwnerReadIndexMap;
		TMap<FNiagaraSystemInstanceID, int32> InstanceOwnerWriteIndexMap;

		TSet<FNiagaraSystemInstanceID> RegisteredInstances;

		int32 QueuedOwnerCount = 0;
		int32 AllocatedOwnerCount = 0;
		int32 ResultsOwnerCount = 0;

		// XY is the grid of sensors
		const uint32 SensorGridLayerCount;

		// Number of float4 channels to reserve for each sensor
		const int32 UserChannelCount;

		FSensorGridNetworkProxy() = delete;

		FSensorGridNetworkProxy(uint32 InSensorGridLayerCount, int32 InUserChannelCount)
			: SensorGridLayerCount(InSensorGridLayerCount)
			, UserChannelCount(InUserChannelCount)
		{
			TransientResources = MakeUnique<FACFSensorGridHelper::FTransientResources>();
		}

		void PrepareSimulation(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type InFeatureLevel)
		{
			FACFSensorGridHelper::FResourceSizingInfo SizingInfo;
			SizingInfo.SensorCount = (1 << SensorGridLayerCount) * (1 << SensorGridLayerCount);
			SizingInfo.OwnerCount = QueuedOwnerCount;
			SizingInfo.UserChannelCount = UserChannelCount;

			AllocatedOwnerCount = QueuedOwnerCount;
			QueuedOwnerCount = 0;

			TransientResources->Build(GraphBuilder,	SizingInfo);
			if (TransientResources->HasBuffers)
			{
				FACFSensorGridHelper Helper(InFeatureLevel, FUintVector4(SensorGridLayerCount, SensorGridLayerCount, AllocatedOwnerCount, 0), GFrameNumberRenderThread);
				Helper.InitBuffers(GraphBuilder, *TransientResources);
			}
		}

		void EndSimulation(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type InFeatureLevel, const FVector2D& GlobalSensorRange)
		{
			FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;

			if (AllocatedOwnerCount)
			{
				const uint32 SensorCountPerSide = 1 << SensorGridLayerCount;

				FVector2D SensorRange = GlobalSensorRange;
				if (GACFSensorGridRadiusOverride > 0.0f)
				{
					SensorRange.Y = GACFSensorGridRadiusOverride;
				}

				if (TransientResources->HasBuffers)
				{
					FACFSensorGridHelper Helper(InFeatureLevel, FUintVector4(SensorGridLayerCount, SensorGridLayerCount, AllocatedOwnerCount, 0), GFrameNumberRenderThread);
					Helper.NearestSensors(GraphBuilder, SensorRange, *TransientResources);

					GraphBuilder.QueueBufferExtraction(TransientResources->TraversalResults, &ExtractedSensorInfo);
					if (UserChannelCount)
					{
						GraphBuilder.QueueBufferExtraction(TransientResources->UserChannelData, &ExtractedUserChannelData);
					}
				}

				InstanceOwnerReadIndexMap = InstanceOwnerWriteIndexMap;
				InstanceOwnerWriteIndexMap.Reset();
			}
		}

	};

	struct FNDIProxy : public FNiagaraDataInterfaceProxy
	{
		FRWLock NetworkLock;
		TMap<const FNiagaraGpuComputeDispatchInterface*, TUniquePtr<FSensorGridNetworkProxy>> NetworkProxies;

		uint32 SensorGridSize = 0;
		FVector2D GlobalSensorRange = FVector2D(EForceInit::ForceInitToZero);
		int32 UserChannelCount = 0;

		int32 PerInstanceDataPassedToRenderThreadSize() const
		{
			return 0;
		}

		FSensorGridNetworkProxy* GetNetwork(const FNiagaraGpuComputeDispatchInterface* Batcher)
		{
			FReadScopeLock _Scope(NetworkLock);
			return NetworkProxies.FindChecked(Batcher).Get();
		}

		void RegisterNetworkInstance(const FNiagaraGpuComputeDispatchInterface* Batcher, FNiagaraSystemInstanceID SystemInstanceID)
		{
			FWriteScopeLock _Scope(NetworkLock);
			TUniquePtr<FSensorGridNetworkProxy>& Network = NetworkProxies.FindOrAdd(Batcher, nullptr);
			if (!Network)
			{
				Network = MakeUnique<FSensorGridNetworkProxy>(SensorGridSize, UserChannelCount);
			}

			Network->RegisteredInstances.Add(SystemInstanceID);
		}

		void UnregisterNetworkInstance(const FNiagaraGpuComputeDispatchInterface* Batcher, FNiagaraSystemInstanceID SystemInstanceID)
		{
			FWriteScopeLock _Scope(NetworkLock);
			if (TUniquePtr<FSensorGridNetworkProxy>* NetworkPtr = NetworkProxies.Find(Batcher))
			{
				if (FSensorGridNetworkProxy* Network = NetworkPtr->Get())
				{
					Network->RegisteredInstances.Remove(SystemInstanceID);
					if (!Network->RegisteredInstances.Num())
					{
						NetworkProxies.Remove(Batcher);
					}
				}
			}
		}

		virtual void PreStage(const FNDIGpuComputePreStageContext& Context) override
		{
			FNiagaraDataInterfaceProxy::PreStage(Context);

			if (FSensorGridNetworkProxy* Network = GetNetwork(&Context.GetComputeDispatchInterface()))
			{
				int32& OwnerIndex = Network->InstanceOwnerWriteIndexMap.FindOrAdd(Context.GetSystemInstanceID(), INDEX_NONE);
				if (OwnerIndex == INDEX_NONE)
				{
					if (ensure(Network->QueuedOwnerCount < ((int32)FACFSensorGridHelper::GetMaxOwnerCount())))
					{
						OwnerIndex = Network->QueuedOwnerCount++;
					}
				}
			}
		}

		virtual bool RequiresPreStageFinalize() const override
		{
			return true;
		}

		virtual void FinalizePreStage(FRDGBuilder& GraphBuilder, const FNiagaraGpuComputeDispatchInterface& ComputeDispatchInterface) override
		{
			FNiagaraDataInterfaceProxy::FinalizePreStage(GraphBuilder, ComputeDispatchInterface);

			if (FSensorGridNetworkProxy* Network = GetNetwork(&ComputeDispatchInterface))
			{
				Network->PrepareSimulation(GraphBuilder, ComputeDispatchInterface.GetFeatureLevel());
			}
		}

		virtual bool RequiresPostStageFinalize() const override
		{
			return true;
		}

		virtual void FinalizePostStage(FRDGBuilder& GraphBuilder, const FNiagaraGpuComputeDispatchInterface& ComputeDispatchInterface) override
		{
			FNiagaraDataInterfaceProxy::FinalizePostStage(GraphBuilder, ComputeDispatchInterface);

			if (FSensorGridNetworkProxy* Network = GetNetwork(&ComputeDispatchInterface))
			{
				Network->EndSimulation(GraphBuilder, ComputeDispatchInterface.GetFeatureLevel(), GlobalSensorRange);
			}
		}

		void RenderThreadInitialize(int32 InSensorCount, const FVector2D& InGlobalSensorRange, int32 InUserChannelCount)
		{
			// SensorCount needs to be a power of two
			SensorGridSize = FMath::Min(FACFSensorGridHelper::GetMaxSensorDensity(), FMath::CeilLogTwo(InSensorCount));
			GlobalSensorRange.X = FMath::Max(0.0f, InGlobalSensorRange.X);
			GlobalSensorRange.Y = FMath::Max(GlobalSensorRange.X, InGlobalSensorRange.Y);
			UserChannelCount = InUserChannelCount;

			NetworkProxies.Reset();
		}
	};
}

UNiagaraDataInterfaceACFSensorGrid::UNiagaraDataInterfaceACFSensorGrid(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
    Proxy.Reset(new NDIACFSensorGridLocal::FNDIProxy());
}

void UNiagaraDataInterfaceACFSensorGrid::PostInitProperties()
{
	Super::PostInitProperties();

	//Can we register data interfaces as regular types and fold them into the FNiagaraVariable framework for UI and function calls etc?
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

void UNiagaraDataInterfaceACFSensorGrid::PostLoad()
{
	Super::PostLoad();

	if (SensorCountPerSide)
	{
		MarkRenderDataDirty();
	}
}

#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceACFSensorGrid::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	{
		FNiagaraFunctionSignature SigUpdateSensor;
		SigUpdateSensor.Name = NDIACFSensorGridLocal::UpdateSensorName;
		SigUpdateSensor.bRequiresExecPin = true;
		SigUpdateSensor.bMemberFunction = true;
		SigUpdateSensor.bRequiresContext = false;
		SigUpdateSensor.bSupportsCPU = false;
		SigUpdateSensor.FunctionVersion = NDIACFSensorGridLocal::NodeVersion_Latest;
		SigUpdateSensor.Description = NDIACFSensorGridLocal::UpdateSensorDescription;

		SigUpdateSensor.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), NDIACFSensorGridLocal::DataInterfacePinName));
		SigUpdateSensor.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Sensor_X")), NDIACFSensorGridLocal::SensorXDescription);
		SigUpdateSensor.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Sensor_Y")), NDIACFSensorGridLocal::SensorYDescription);
		SigUpdateSensor.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Location")), NDIACFSensorGridLocal::LocationDescription);
		SigUpdateSensor.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("SensorRange")), NDIACFSensorGridLocal::SensorRangeDescription);
		SigUpdateSensor.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsValid")), NDIACFSensorGridLocal::SensorValidDescription);
		OutFunctions.Add(SigUpdateSensor);
	}

	{
		FNiagaraFunctionSignature SigFindNearest;
		SigFindNearest.Name = NDIACFSensorGridLocal::FindNearestName;
		SigFindNearest.bMemberFunction = true;
		SigFindNearest.bRequiresContext = false;
		SigFindNearest.bSupportsCPU = false;
		SigFindNearest.FunctionVersion = NDIACFSensorGridLocal::NodeVersion_Latest;
		SigFindNearest.Description = NDIACFSensorGridLocal::FindNearestDescription;

		SigFindNearest.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), NDIACFSensorGridLocal::DataInterfacePinName));
		SigFindNearest.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Sensor_X")), NDIACFSensorGridLocal::SensorXDescription);
		SigFindNearest.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Sensor_Y")), NDIACFSensorGridLocal::SensorYDescription);
		SigFindNearest.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Out_Location")), NDIACFSensorGridLocal::OutputLocationDescription);
		SigFindNearest.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Out_Distance")), NDIACFSensorGridLocal::OutputDistanceDescription);
		SigFindNearest.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Out_SensorX")),
			NDIACFSensorGridLocal::OutputSensorXDescription);
		SigFindNearest.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Out_SensorY")),
			NDIACFSensorGridLocal::OutputSensorYDescription);
		SigFindNearest.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Out_OwnerIndex")),
			NDIACFSensorGridLocal::OutputOwnerIndexDescription);
		SigFindNearest.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Out_IsValid")), NDIACFSensorGridLocal::OutputIsValidDescription);
		OutFunctions.Add(SigFindNearest);
	}

	{
		FNiagaraFunctionSignature SigReadUserData;
		SigReadUserData.Name = NDIACFSensorGridLocal::ReadUserChannelName;
		SigReadUserData.bMemberFunction = true;
		SigReadUserData.bRequiresContext = false;
		SigReadUserData.bSupportsCPU = false;
		SigReadUserData.FunctionVersion = NDIACFSensorGridLocal::NodeVersion_Latest;
		SigReadUserData.Description = NDIACFSensorGridLocal::ReadUserChannelDescription;

		SigReadUserData.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), NDIACFSensorGridLocal::DataInterfacePinName));
		SigReadUserData.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Sensor_X")), NDIACFSensorGridLocal::SensorXDescription);
		SigReadUserData.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Sensor_Y")), NDIACFSensorGridLocal::SensorYDescription);
		SigReadUserData.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("OwnerIndex")),
			NDIACFSensorGridLocal::OwnerIndexDescription);
		SigReadUserData.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Channel")), NDIACFSensorGridLocal::UserChannelIndexDescription);
		SigReadUserData.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Out_UserData")), NDIACFSensorGridLocal::OutputUserChannelDescription);
		SigReadUserData.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Out_IsValid")), NDIACFSensorGridLocal::OutputIsValidDescription);
		OutFunctions.Add(SigReadUserData);
	}

	{
		FNiagaraFunctionSignature SigWriteUserData;
		SigWriteUserData.Name = NDIACFSensorGridLocal::WriteUserChannelName;
		SigWriteUserData.bRequiresExecPin = true;
		SigWriteUserData.bMemberFunction = true;
		SigWriteUserData.bRequiresContext = false;
		SigWriteUserData.bSupportsCPU = false;
		SigWriteUserData.FunctionVersion = NDIACFSensorGridLocal::NodeVersion_Latest;
		SigWriteUserData.Description = NDIACFSensorGridLocal::WriteUserChannelDescription;

		SigWriteUserData.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), NDIACFSensorGridLocal::DataInterfacePinName));
		SigWriteUserData.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Sensor_X")), NDIACFSensorGridLocal::SensorXDescription);
		SigWriteUserData.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Sensor_Y")), NDIACFSensorGridLocal::SensorYDescription);
		SigWriteUserData.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Channel")), NDIACFSensorGridLocal::UserChannelIndexDescription);
		SigWriteUserData.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Value")), NDIACFSensorGridLocal::UserChannelValueDescription);
		OutFunctions.Add(SigWriteUserData);
	}
}

bool UNiagaraDataInterfaceACFSensorGrid::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
{
	if ( (FunctionInfo.DefinitionName == NDIACFSensorGridLocal::UpdateSensorName) ||
		(FunctionInfo.DefinitionName == NDIACFSensorGridLocal::FindNearestName) ||
		(FunctionInfo.DefinitionName == NDIACFSensorGridLocal::ReadUserChannelName) ||
		(FunctionInfo.DefinitionName == NDIACFSensorGridLocal::WriteUserChannelName))
	{
		return true;
	}

	return false;
}

void UNiagaraDataInterfaceACFSensorGrid::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{TEXT("ParameterName"),	ParamInfo.DataInterfaceHLSLSymbol},
	};

	AppendTemplateHLSL(OutHLSL, NDIACFSensorGridLocal::TemplateShaderFile, TemplateArgs);
}

void UNiagaraDataInterfaceACFSensorGrid::GetCommonHLSL(FString& OutHlsl)
{
	OutHlsl.Appendf(TEXT("#include \"%s\"\n"), NDIACFSensorGridLocal::CommonShaderFile);
}

bool UNiagaraDataInterfaceACFSensorGrid::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	bool bSuccess = Super::AppendCompileHash(InVisitor);

	bSuccess &= InVisitor->UpdateShaderFile(NDIACFSensorGridLocal::CommonShaderFile);
	bSuccess &= InVisitor->UpdateShaderFile(NDIACFSensorGridLocal::TemplateShaderFile);
	bSuccess &= InVisitor->UpdateShaderParameters<NDIACFSensorGridLocal::FShaderParameters>();

	return bSuccess;
}

bool UNiagaraDataInterfaceACFSensorGrid::UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature)
{
	bool WasChanged = false;

	if (FunctionSignature.Name == NDIACFSensorGridLocal::UpdateSensorName && FunctionSignature.FunctionVersion == NDIACFSensorGridLocal::NodeVersion::NodeVersion_Initial)
	{
		FunctionSignature.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsValid")), NDIACFSensorGridLocal::SensorValidDescription);
		FunctionSignature.FunctionVersion = NDIACFSensorGridLocal::NodeVersion_Latest;

		WasChanged = true;
	}

	if (FunctionSignature.Name == NDIACFSensorGridLocal::FindNearestName)
	{
		if (FunctionSignature.FunctionVersion == NDIACFSensorGridLocal::NodeVersion::NodeVersion_Initial
			|| FunctionSignature.FunctionVersion == NDIACFSensorGridLocal::NodeVersion::Nodeversion_Reworked_Output)
		{
			FunctionSignature.Outputs.Reset();
			FunctionSignature.OutputDescriptions.Reset();

			FunctionSignature.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Out_Location")), NDIACFSensorGridLocal::OutputLocationDescription);
			FunctionSignature.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Out_Distance")), NDIACFSensorGridLocal::OutputDistanceDescription);
			FunctionSignature.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Out_SensorX")),
			NDIACFSensorGridLocal::OutputSensorXDescription);
			FunctionSignature.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Out_SensorY")),
			NDIACFSensorGridLocal::OutputSensorYDescription);
			FunctionSignature.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Out_OwnerIndex")),
			NDIACFSensorGridLocal::OutputOwnerIndexDescription);
			FunctionSignature.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Out_IsValid")), NDIACFSensorGridLocal::OutputIsValidDescription);
			FunctionSignature.FunctionVersion = NDIACFSensorGridLocal::NodeVersion_Latest;

			WasChanged = true;
		}
	}

	return WasChanged;
}

#endif

int32 UNiagaraDataInterfaceACFSensorGrid::PerInstanceDataSize() const
{
	return 4;
}

bool UNiagaraDataInterfaceACFSensorGrid::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	if (SystemInstance)
	{
		NDIACFSensorGridLocal::FNDIProxy* RT_Proxy = GetProxyAs<NDIACFSensorGridLocal::FNDIProxy>();

		// Push Updates to Proxy
		ENQUEUE_RENDER_COMMAND(FRegisterInstance)(
			[RT_Proxy, RT_Batcher = SystemInstance->GetComputeDispatchInterface(), RT_InstanceID = SystemInstance->GetId()](FRHICommandListImmediate& RHICmdList)
		{
			RT_Proxy->RegisterNetworkInstance(RT_Batcher, RT_InstanceID);
		});
	}

	return true;
}

void UNiagaraDataInterfaceACFSensorGrid::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	if (SystemInstance)
	{
		NDIACFSensorGridLocal::FNDIProxy* RT_Proxy = GetProxyAs<NDIACFSensorGridLocal::FNDIProxy>();

		// Push Updates to Proxy
		ENQUEUE_RENDER_COMMAND(FUnregisterInstance)(
			[RT_Proxy, RT_Batcher = SystemInstance->GetComputeDispatchInterface(), RT_InstanceID = SystemInstance->GetId()](FRHICommandListImmediate& RHICmdList)
		{
			RT_Proxy->UnregisterNetworkInstance(RT_Batcher, RT_InstanceID);
		});
	}
}

bool UNiagaraDataInterfaceACFSensorGrid::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	const UNiagaraDataInterfaceACFSensorGrid* OtherTyped = CastChecked<const UNiagaraDataInterfaceACFSensorGrid>(Other);
	return OtherTyped->SensorCountPerSide == SensorCountPerSide
		&& OtherTyped->GlobalSensorAccuracy == GlobalSensorAccuracy
		&& OtherTyped->GlobalSensorRange == GlobalSensorRange
		&& OtherTyped->UserChannelCount == UserChannelCount;
}

bool UNiagaraDataInterfaceACFSensorGrid::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceACFSensorGrid* OtherTyped = CastChecked<UNiagaraDataInterfaceACFSensorGrid>(Destination);
	OtherTyped->SensorCountPerSide = SensorCountPerSide;
	OtherTyped->GlobalSensorAccuracy = GlobalSensorAccuracy;
	OtherTyped->GlobalSensorRange = GlobalSensorRange;
	OtherTyped->UserChannelCount = UserChannelCount;
	OtherTyped->MarkRenderDataDirty();
	return true;
}

void UNiagaraDataInterfaceACFSensorGrid::PushToRenderThreadImpl()
{
	NDIACFSensorGridLocal::FNDIProxy* RT_Proxy = GetProxyAs<NDIACFSensorGridLocal::FNDIProxy>();

	// Push Updates to Proxy, first release any resources
	ENQUEUE_RENDER_COMMAND(FUpdateDI)(
		[RT_Proxy,
		RT_SensorCount = SensorCountPerSide,
		RT_GlobalSensorRange = FVector2D(GlobalSensorAccuracy, GlobalSensorRange),
		RT_UserChannelCount = UserChannelCount](FRHICommandListImmediate& RHICmdList)
		{
			RT_Proxy->RenderThreadInitialize(RT_SensorCount, RT_GlobalSensorRange, RT_UserChannelCount);
		});
}

void UNiagaraDataInterfaceACFSensorGrid::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<NDIACFSensorGridLocal::FShaderParameters>();
}

void UNiagaraDataInterfaceACFSensorGrid::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	using namespace NDIACFSensorGridLocal;

	FRDGBuilder& GraphBuilder = Context.GetGraphBuilder();
	FNDIProxy& DataInterfaceProxy = Context.GetProxy<FNDIProxy>();
	FSensorGridNetworkProxy* NetworkProxy = DataInterfaceProxy.GetNetwork(&Context.GetComputeDispatchInterface());
	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();

	// access to the shared buffers of sensor locations & user channel data should be localized for each instance of the sensor simulation
	// and so we enable UAV overlap
	const bool bEnableUAVOverlap = true;
	const ERDGUnorderedAccessViewFlags UAVFlags = bEnableUAVOverlap ? ERDGUnorderedAccessViewFlags::SkipBarrier : ERDGUnorderedAccessViewFlags::None;

	if (Context.IsResourceBound(&ShaderParameters->RWSensorLocations))
	{
		ShaderParameters->RWSensorLocations = NetworkProxy->TransientResources->SensorLocations
			? GraphBuilder.CreateUAV(NetworkProxy->TransientResources->SensorLocations, PF_A32B32G32R32F, UAVFlags)
			: Context.GetComputeDispatchInterface().GetEmptyBufferUAV(GraphBuilder, PF_A32B32G32R32F);
	}

	if (Context.IsResourceBound(&ShaderParameters->SensorInfo))
	{
		if (NetworkProxy->ExtractedSensorInfo)
		{
			ShaderParameters->SensorInfo = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(NetworkProxy->ExtractedSensorInfo));
		}
		else
		{
			FACFSensorGridHelper::FSensorInfo DummySensorInfoData{};
			FRDGBufferRef DummySensorInfoBuffer = CreateStructuredBuffer<FACFSensorGridHelper::FSensorInfo>(GraphBuilder, TEXT("ACFSensorGridDummySensorInfo"), { DummySensorInfoData });
			ShaderParameters->SensorInfo = GraphBuilder.CreateSRV(DummySensorInfoBuffer);
		}
	}

	if (Context.IsResourceBound(&ShaderParameters->RWSensorGridUserChannelData))
	{
		ShaderParameters->RWSensorGridUserChannelData = NetworkProxy->TransientResources->UserChannelData
			? GraphBuilder.CreateUAV(NetworkProxy->TransientResources->UserChannelData, PF_A32B32G32R32F, UAVFlags)
			: Context.GetComputeDispatchInterface().GetEmptyBufferUAV(GraphBuilder, PF_A32B32G32R32F);
	}

	if (Context.IsResourceBound(&ShaderParameters->SensorGridPreviousUserChannelData))
	{
		ShaderParameters->SensorGridPreviousUserChannelData = NetworkProxy->ExtractedUserChannelData
			? GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(NetworkProxy->ExtractedUserChannelData))
			: Context.GetComputeDispatchInterface().GetEmptyBufferSRV(GraphBuilder, PF_A32B32G32R32F);
	}

	const FUintVector4 SensorGridDimensions(NetworkProxy->SensorGridLayerCount, NetworkProxy->SensorGridLayerCount, NetworkProxy->AllocatedOwnerCount, 1);

	// we tag the read dimensions as being invalid (w component being 0) so that ee don't even bother reading from the empty srv
	ShaderParameters->ReadSensorGridDimensions = NetworkProxy->ExtractedSensorInfo ? SensorGridDimensions : FUintVector4(0, 0, 0, 0);
	ShaderParameters->WriteSensorGridDimensions = NetworkProxy->TransientResources->HasBuffers ? SensorGridDimensions : FUintVector4(0, 0, 0, 0);

	const int32* WriteIndexPtr = NetworkProxy->InstanceOwnerWriteIndexMap.Find(Context.GetSystemInstanceID());
	ShaderParameters->SensorGridWriteIndex = WriteIndexPtr ? *WriteIndexPtr : INDEX_NONE;

	const int32* ReadIndexPtr = NetworkProxy->InstanceOwnerReadIndexMap.Find(Context.GetSystemInstanceID());
	ShaderParameters->SensorGridReadIndex = ReadIndexPtr ? *ReadIndexPtr : INDEX_NONE;

	ShaderParameters->SensorGridUserChannelCount = NetworkProxy->UserChannelCount;
}

#if WITH_EDITOR
void UNiagaraDataInterfaceACFSensorGrid::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property &&
		(PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceACFSensorGrid, SensorCountPerSide)
		|| PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceACFSensorGrid, GlobalSensorAccuracy)
		|| PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceACFSensorGrid, GlobalSensorRange)
		|| PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceACFSensorGrid, UserChannelCount)))
	{
		MarkRenderDataDirty();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
