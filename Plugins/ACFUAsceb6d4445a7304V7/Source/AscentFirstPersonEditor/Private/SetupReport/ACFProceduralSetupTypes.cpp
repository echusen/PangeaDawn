// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "SetupReport/ACFProceduralSetupTypes.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Components/ACFProceduralMotionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Presets/ACFProceduralPreset.h"

#define LOCTEXT_NAMESPACE "ACFProceduralSetupReport"

namespace ACFProceduralSetupContext
{
	/** Every editable UACFProceduralPresetData-typed property on Object, whether set or not. */
	static void CollectPresetBindings(UObject* Object, TArray<FACFProceduralPresetBinding>& OutBindings)
	{
		if (!::IsValid(Object))
		{
			return;
		}

		for (TFieldIterator<FObjectPropertyBase> It(Object->GetClass()); It; ++It)
		{
			FObjectPropertyBase* Property = *It;
			if (!Property->PropertyClass || !Property->PropertyClass->IsChildOf(UACFProceduralPresetData::StaticClass()))
			{
				continue;
			}

			// A binding is something the user can set. Internal caches of the same type — the component's
			// resolved InheritedPresetData, for one — are not bindings and must not be offered as such.
			if (!Property->HasAnyPropertyFlags(CPF_Edit))
			{
				continue;
			}

			FACFProceduralPresetBinding Binding;
			Binding.Owner = Object;
			Binding.Property = Property;
			Binding.PresetData = Cast<UACFProceduralPresetData>(Property->GetObjectPropertyValue_InContainer(Object));

			OutBindings.Add(Binding);
		}
	}

	/**
	 * The template a leaf class actually uses for an inherited SCS node.
	 *
	 * A child blueprint that edits an inherited component stores the edited copy in its own
	 * InheritableComponentHandler, not in the parent's SCS node — so reading the node's template directly
	 * sees the parent's untouched defaults.
	 */
	static UActorComponent* ResolveComponentTemplate(UClass* LeafClass, USCS_Node* Node)
	{
		if (!Node)
		{
			return nullptr;
		}

		const FComponentKey Key(Node);

		for (UClass* Current = LeafClass; Current; Current = Current->GetSuperClass())
		{
			UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Current);
			if (!Generated)
			{
				continue;
			}

			if (const UInheritableComponentHandler* Handler = Generated->GetInheritableComponentHandler(false))
			{
				if (UActorComponent* Override = Handler->GetOverridenComponentTemplate(Key))
				{
					return Override;
				}
			}
		}

		return Node->ComponentTemplate;
	}

	/** Native components on the CDO plus every SCS template up the class hierarchy. */
	static void CollectComponentTemplates(UClass* Class, TArray<UActorComponent*>& OutComponents)
	{
		if (!Class)
		{
			return;
		}

		if (const AActor* DefaultActor = Cast<AActor>(Class->GetDefaultObject()))
		{
			TArray<UActorComponent*> NativeComponents;
			DefaultActor->GetComponents(NativeComponents);
			OutComponents.Append(NativeComponents);
		}

		for (UClass* Current = Class; Current; Current = Current->GetSuperClass())
		{
			const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Current);
			if (!Generated || !Generated->SimpleConstructionScript)
			{
				continue;
			}

			for (USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
			{
				if (UActorComponent* Template = ResolveComponentTemplate(Class, Node))
				{
					OutComponents.AddUnique(Template);
				}
			}
		}
	}

	/** Resolves the anim blueprint behind a skeletal mesh component's anim class, if it has one. */
	static UAnimBlueprint* GetAnimBlueprint(const USkeletalMeshComponent* Component)
	{
		if (!Component || !Component->AnimClass)
		{
			return nullptr;
		}

		const UAnimBlueprintGeneratedClass* Generated = Cast<UAnimBlueprintGeneratedClass>(Component->AnimClass);
		return Generated ? Cast<UAnimBlueprint>(Generated->ClassGeneratedBy) : nullptr;
	}
}

FString GetACFProceduralFriendlyName(const UObject* Object)
{
	if (!Object)
	{
		return FString();
	}

	FString Name = Object->GetName();
	Name.RemoveFromEnd(TEXT("_GEN_VARIABLE"));

	return Name;
}

FText FACFProceduralPresetBinding::GetDisplayName() const
{
	const UObject* OwnerObject = Owner.Get();
	if (!OwnerObject || !Property)
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		LOCTEXT("PresetBindingName", "{0} · {1}"),
		FText::FromString(GetACFProceduralFriendlyName(OwnerObject)),
		Property->GetDisplayNameText());
}

void FACFProceduralSetupContext::Build(UObject* InTarget)
{
	using namespace ACFProceduralSetupContext;

	*this = FACFProceduralSetupContext();

	if (!::IsValid(InTarget))
	{
		return;
	}

	Target = InTarget;

	// Categorise whatever was handed in, then reflect over everything it reaches.
	TArray<UObject*> Owners;
	TArray<UActorComponent*> Components;

	if (USkeleton* AsSkeleton = Cast<USkeleton>(InTarget))
	{
		Skeleton = AsSkeleton;
	}
	else if (USkeletalMesh* AsMesh = Cast<USkeletalMesh>(InTarget))
	{
		Skeleton = AsMesh->GetSkeleton();
	}
	else if (UAnimBlueprint* AsAnimBlueprint = Cast<UAnimBlueprint>(InTarget))
	{
		Skeleton = AsAnimBlueprint->TargetSkeleton;
		AnimBlueprints.Add(AsAnimBlueprint);
	}
	else if (UACFProceduralPresetData* AsPresetData = Cast<UACFProceduralPresetData>(InTarget))
	{
		PresetDataAssets.Add(AsPresetData);
	}
	else if (AActor* AsActor = Cast<AActor>(InTarget))
	{
		bIsLiveActor = true;

		// Equipping means attaching, so attached actors carry components that belong to this setup.
		TArray<AActor*> Actors;
		Actors.Add(AsActor);
		AsActor->GetAttachedActors(Actors, false, true);

		for (AActor* Actor : Actors)
		{
			if (!::IsValid(Actor))
			{
				continue;
			}

			Owners.Add(Actor);

			TArray<UActorComponent*> ActorComponents;
			Actor->GetComponents(ActorComponents);
			Components.Append(ActorComponents);
		}
	}
	else if (const UBlueprint* AsBlueprint = Cast<UBlueprint>(InTarget))
	{
		if (UClass* Generated = AsBlueprint->GeneratedClass)
		{
			Owners.Add(Generated->GetDefaultObject());
			CollectComponentTemplates(Generated, Components);
		}
	}

	for (UActorComponent* Component : Components)
	{
		if (!::IsValid(Component))
		{
			continue;
		}

		Owners.Add(Component);

		if (UACFProceduralMotionComponent* MotionComponent = Cast<UACFProceduralMotionComponent>(Component))
		{
			MotionComponents.Add(MotionComponent);
		}

		USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(Component);
		if (!MeshComponent)
		{
			continue;
		}

		if (!Skeleton.IsValid())
		{
			if (USkeletalMesh* Mesh = MeshComponent->GetSkeletalMeshAsset())
			{
				Skeleton = Mesh->GetSkeleton();
			}
		}

		if (UAnimBlueprint* AnimBlueprint = GetAnimBlueprint(MeshComponent))
		{
			AnimBlueprints.AddUnique(AnimBlueprint);
		}
	}

	for (UObject* Owner : Owners)
	{
		CollectPresetBindings(Owner, PresetBindings);
	}

	for (const FACFProceduralPresetBinding& Binding : PresetBindings)
	{
		if (Binding.PresetData.IsValid())
		{
			PresetDataAssets.AddUnique(Binding.PresetData);
		}
	}

	// An anim blueprint reached through a mesh is the best source of a skeleton when nothing else had one.
	if (!Skeleton.IsValid())
	{
		for (const TWeakObjectPtr<UAnimBlueprint>& AnimBlueprint : AnimBlueprints)
		{
			if (AnimBlueprint.IsValid() && AnimBlueprint->TargetSkeleton)
			{
				Skeleton = AnimBlueprint->TargetSkeleton;
				break;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
