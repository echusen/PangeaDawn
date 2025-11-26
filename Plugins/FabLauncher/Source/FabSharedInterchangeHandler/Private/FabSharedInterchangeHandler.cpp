// Copyright Epic Games, Inc. All Rights Reserved.

#include "FabSharedInterchangeHandler.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeGenericScenesPipeline.h"
#include "InterchangeManager.h"
#include "InterchangeProjectSettings.h"
#include "InterchangeSceneNode.h"
#include "InterchangeSourceData.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "HAL/IConsoleManager.h"

#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION <= 4)
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "USDStageImportOptions.h"
#endif

const FSoftObjectPath GetGenericLevelPipelinePath()
{
	FSoftObjectPath LevelPipelinePath;
	const UInterchangeProjectSettings* Settings = GetDefault<UInterchangeProjectSettings>();
	const FInterchangePipelineStack* Stack = Settings->SceneImportSettings.PipelineStacks.Find(Settings->SceneImportSettings.DefaultPipelineStack);
	if (Stack)
	{
		for(const FSoftObjectPath& PipelinePath : Stack->Pipelines)
		{
			FString PipelineName = PipelinePath.ToString();
			UInterchangePipelineBase* const Pipeline = Cast<UInterchangePipelineBase>(PipelinePath.TryLoad());
			if (!Pipeline)
			{
				UE_LOG(LogTemp, Error, TEXT("Pipeline %s could not be loaded, this should not happen"), *PipelineName);
				continue;
			}
			if (UInterchangePipelineBase* GeneratedPipeline = UE::Interchange::GeneratePipelineInstance(PipelineName))
			{
				if(UInterchangeGenericLevelPipeline* LevelPipeline = Cast<UInterchangeGenericLevelPipeline>(GeneratedPipeline))
				{
					LevelPipelinePath = PipelinePath;
					break;
				}
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid default pipeline stack, this should not happen"));
	}
	return LevelPipelinePath;
}

bool FileHasInstancesOrComplexHierarchy(const UInterchangeSourceData* SourceData)
{
	// Translate the file source data into a temporary Base Container
    UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
    if (!InterchangeManager.CanTranslateSourceData(SourceData))
	{
		UE_LOG(LogTemp, Warning, TEXT("No translator, this should not happen"));
		return false;
	}
    UInterchangeTranslatorBase* Translator = InterchangeManager.GetTranslatorForSourceData(SourceData);
    if (!Translator) {
		return false;
	}

    UInterchangeResultsContainer* ResultsContainer = NewObject<UInterchangeResultsContainer>(GetTransientPackage());
    Translator->SetResultsContainer(ResultsContainer);
    UInterchangeBaseNodeContainer* Container = NewObject<UInterchangeBaseNodeContainer>(GetTransientPackage());
    if (!Translator->Translate(*Container)) {
		UE_LOG(LogTemp, Error, TEXT("Translate failed"));
		return false;
	}

    // Build & print a scene tree
    // Find roots = scene nodes with no parent
	bool bMultipleInstancesOfSameMeshes = false;
	bool bDifferentMeshParents = false;
	{
		FString UniqueParent = "";
		TMap<FString, FString> NodeUidToInstanceUid;
		Container->IterateNodesOfType<UInterchangeSceneNode>(
			[&](const FString& Uid, UInterchangeSceneNode* SceneNode)
			{
				FString AssetInstanceUid;
				if (SceneNode->GetCustomAssetInstanceUid(AssetInstanceUid) && !AssetInstanceUid.IsEmpty())
				{
					FString Parent = SceneNode->GetParentUid();

					// Check if the node is a top level parent
					if(!bDifferentMeshParents)
					{
						if (!Parent.IsEmpty()) 
						{
							if (UniqueParent.IsEmpty())
							{
								UniqueParent = Parent;
							}
							else if (UniqueParent != Parent)
							{
								// Two nodes referencing meshes have different parents, flag it
								bDifferentMeshParents = true;
							}
						}
						else
						{
							// When parent is empty, we have a root node. Could this be useful for detection ?
						}
					}
	
					// Check if the referenced mesh is already referenced
					if(NodeUidToInstanceUid.Find(AssetInstanceUid))
					{
						// The instance uid was already registered, we have instances
						bMultipleInstancesOfSameMeshes = true;
					}
					else
					{
						NodeUidToInstanceUid.Add({AssetInstanceUid, Uid});
					}

					// For such meshes, only continue iteration if both flags are false
					return bDifferentMeshParents || bMultipleInstancesOfSameMeshes;
				}
				return true;
			}
		);
	}

	return bDifferentMeshParents || bMultipleInstancesOfSameMeshes;
}

void ImportFabAssetWithInterchange(
	const FString& Source,
	const FString& Destination, 
	TFunction<void(TArray<UInterchangePipelineBase*>&, bool)> AdjustPipelinesFunction,
	TFunction<void(const TArray<UObject*>&)> Callback
)
{
	// Separately handle USD import up to UE 5.5
	#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION <= 4)
	FString Extension = FPaths::GetExtension(Source).ToLower();
	if ((Extension == TEXT("usd")) || (Extension == TEXT("usda")) || (Extension == TEXT("usdc")) || (Extension == TEXT("usdz")))
	{
		TSharedPtr<TArray<UAssetImportTask*>> MeshImportTasks = MakeShared<TArray<UAssetImportTask*>>();
		UAssetImportTask* MeshImportTask = NewObject<UAssetImportTask>();
		MeshImportTask->AddToRoot();
		MeshImportTask->bAutomated = true;
		MeshImportTask->bSave      = false;
		MeshImportTask->Filename   = Source;
		#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION > 0)
		MeshImportTask->bAsync     = true;
		#endif
		MeshImportTask->DestinationPath  = Destination;
		MeshImportTask->bReplaceExisting = true;
		MeshImportTask->Options          = NewObject<UUsdStageImportOptions>(MeshImportTask);
		MeshImportTasks->Add(MeshImportTask);

		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.ImportAssetTasks(*MeshImportTasks);

		TSharedPtr<TArray<UObject*>> ImportedObjects = MakeShared<TArray<UObject*>>();

		Async(
			EAsyncExecution::Thread,
			[MeshImportTasks, ImportedObjects]()
			{
				for (const UAssetImportTask* const MeshImportTask : *MeshImportTasks)
				{
					#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION > 0)
					if (MeshImportTask->AsyncResults.IsValid())
					{
						FPlatformProcess::ConditionalSleep([=]() { return MeshImportTask->IsAsyncImportComplete(); }, 0.25f);
						ImportedObjects->Append(MeshImportTask->AsyncResults->GetImportedObjects());
					}
					else
					{
						ImportedObjects->Append(MeshImportTask->GetObjects());
					}
					#else
					ImportedObjects->Append(MeshImportTask->Result);
					#endif
				}
			},
			[MeshImportTasks, ImportedObjects, Callback, Destination]()
			{
				Async(
					EAsyncExecution::TaskGraphMainThread,
					[MeshImportTasks, ImportedObjects, Callback, Destination]()
					{
						Callback(*ImportedObjects);
						for (UAssetImportTask* MeshImportTask : *MeshImportTasks)
						{
							MeshImportTask->RemoveFromRoot();
						}
					}
				);
			}
		);
		return;
	}
	#endif

	IConsoleVariable* FbxFlag = IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.FBX"));
	IConsoleVariable* ObjFlag = IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.OBJ"));
	IConsoleVariable* UsdFlag = IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.USD")); // Starting 5.5
	if(UsdFlag) UsdFlag->Set(true, ECVF_SetByCode);
	if(FbxFlag) FbxFlag->Set(true, ECVF_SetByCode);
	if(ObjFlag) ObjFlag->Set(true, ECVF_SetByCode);

	UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
	const UInterchangeSourceData* InitialSourceData = UInterchangeManager::CreateSourceData(Source);
	bool bHasInstancesOrComplexHierarchy = FileHasInstancesOrComplexHierarchy(InitialSourceData);

	// Get the default pipelines for our imports
	TArray<UInterchangePipelineBase*> GeneratedPipelines;
	const FName PipelineStackName                               = FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(false, *InitialSourceData);
	const FInterchangeImportSettings& InterchangeImportSettings = FInterchangeProjectSettingsUtils::GetDefaultImportSettings(false);
	if (const FInterchangePipelineStack* const PipelineStack = InterchangeImportSettings.PipelineStacks.Find(PipelineStackName))
	{
		const TArray<FSoftObjectPath>* StackPipelines = &PipelineStack->Pipelines;
		UE::Interchange::FScopedTranslator ScopedTranslator(InitialSourceData);
		for (const FInterchangeTranslatorPipelines& TranslatorPipelines : PipelineStack->PerTranslatorPipelines)
		{
			const UClass* TranslatorClass = TranslatorPipelines.Translator.LoadSynchronous();
			if (ScopedTranslator.GetTranslator() && ScopedTranslator.GetTranslator()->IsA(TranslatorClass))
			{
				StackPipelines = &TranslatorPipelines.Pipelines;
				break;
			}
		}
		for (const FSoftObjectPath& Pipeline : *StackPipelines)
		{
			UInterchangePipelineBase* const DefaultPipeline = Cast<UInterchangePipelineBase>(Pipeline.TryLoad());
			if (!DefaultPipeline)
			{
				continue;
			}
			if (UInterchangePipelineBase* GeneratedPipeline = UE::Interchange::GeneratePipelineInstance(Pipeline))
			{
				// GeneratedPipeline->AddToRoot(); // Not sure this is needed anymore
				GeneratedPipeline->TransferAdjustSettings(DefaultPipeline);
				GeneratedPipelines.Add(GeneratedPipeline);
			}
		}
	
		if(bHasInstancesOrComplexHierarchy)
		{
			FSoftObjectPath LevelPipelinePath = GetGenericLevelPipelinePath();
			if(!LevelPipelinePath.IsNull())
			{
				if (UInterchangePipelineBase* GeneratedPipeline = UE::Interchange::GeneratePipelineInstance(LevelPipelinePath))
				{
					// #if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)
					// if(UInterchangeGenericLevelPipeline* LevelPipeline = Cast<UInterchangeGenericLevelPipeline>(GeneratedPipeline))
					// {
					// 	LevelPipeline->SceneHierarchyType = EInterchangeSceneHierarchyType::CreatePackedActor;
					// }
					// #endif
					GeneratedPipelines.Add(GeneratedPipeline);
				}
			}
		}
	}

	// Adapt the pipeline
	AdjustPipelinesFunction(GeneratedPipelines, bHasInstancesOrComplexHierarchy);

	// Import parameters
	FImportAssetParameters ImportAssetParameters;
	ImportAssetParameters.bIsAutomated      = true;
	ImportAssetParameters.OverridePipelines = TArray<FSoftObjectPath>(GeneratedPipelines);

	// Trigger the import
	if (bHasInstancesOrComplexHierarchy)
	{
		auto [AssetResult, SceneResult] = InterchangeManager.ImportSceneAsync(
			Destination,
			InitialSourceData,
			ImportAssetParameters
		);
		SceneResult->OnDone([Destination, Callback, Source, Pipelines = MoveTemp(GeneratedPipelines)](const UE::Interchange::FImportResult& Result)
		{
			const UE::Interchange::FImportResult::EStatus Status = Result.GetStatus();
			if (Status == UE::Interchange::FImportResult::EStatus::Done)
			{
				Callback(Result.GetImportedObjects());
				#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)
				// Remove the level actor from the level and content browser
				for(UObject* Object : Result.GetImportedObjects())
				{
					AActor* Actor = Cast<AActor>(Object);
					APackedLevelActor* PackedLevelActor = Cast<APackedLevelActor>(Object);
					if (PackedLevelActor)
					{
						if (UWorld* OwningWorld = Actor->GetWorld())
						{
							OwningWorld->EditorDestroyActor(Actor, true);
							Actor->UObject::Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
						}
					}
				}
				#else
				TArray<AActor*> SpawnedActors;
				for(UObject* Object : Result.GetImportedObjects())
				{
					AActor* Actor = Cast<AActor>(Object);
					SpawnedActors.Add(Actor);
				}
				const FString PackagePath = Destination / FPaths::GetBaseFilename(Source);
				FKismetEditorUtilities::FCreateBlueprintFromActorsParams Params(SpawnedActors);
				Params.bReplaceActors = true;
				Params.ParentClass = APackedLevelActor::StaticClass();
				Params.bOpenBlueprint = false;
				UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromActors(PackagePath, Params);
				#endif
			}
			else if (Status == UE::Interchange::FImportResult::EStatus::Invalid)
			{
				UE_LOG(LogTemp, Error, TEXT("Interchange import failed"));
				Callback({});
			}
		});
	}
	else
	{
		UE::Interchange::FAssetImportResultRef AssetResult = InterchangeManager.ImportAssetAsync(Destination, InitialSourceData, ImportAssetParameters);
		AssetResult->OnDone(
			[Callback, Pipelines = MoveTemp(GeneratedPipelines)](const UE::Interchange::FImportResult& Result)
			{
				const UE::Interchange::FImportResult::EStatus Status = Result.GetStatus();
				if (Status == UE::Interchange::FImportResult::EStatus::Done)
				{
					Callback(Result.GetImportedObjects());
				}
				else if (Status == UE::Interchange::FImportResult::EStatus::Invalid)
				{
					Callback({});
				}
			}
		);
	}
}