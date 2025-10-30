// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#include "ACFSkillTreeComponent.h"

#include "ACFGASAttributesComponent.h"
#include "ARSLevelingComponent.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Graph/ACFBaseSkillNode.h"
#include "Graph/ACFSkillTreeGraph.h"
#include <ACFRPGFunctionLibrary.h>
#include <GameplayTagContainer.h>
#include <Logging.h>
#include <Net/UnrealNetwork.h>
#include <UObject/CoreNet.h>

// Sets default values for this component's properties
UACFSkillTreeComponent::UACFSkillTreeComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.
    // You can turn these features off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UACFSkillTreeComponent::BeginPlay()
{
    Super::BeginPlay();
    AttributeComp = GetOwner()->FindComponentByClass<UACFGASAttributesComponent>();
    AbilityComp = GetOwner()->FindComponentByClass<UACFAbilitySystemComponent>();
}

void UACFSkillTreeComponent::SynchTrees()
{
    // Synchronizes the graph data with the actual one in the component
    for (UACFSkillTreeGraph* Tree : SkillTrees) {
        for (UAGSGraphNode* Node : Tree->GetAllNodes()) {
            UACFBaseSkillNode* SkillNode = Cast<UACFBaseSkillNode>(Node);
            if (!SkillNode) {
                continue;
            }

            const FGuid NodeId = SkillNode->GetNodeId();
            
            if (ActiveSkills.Contains(NodeId) && !SkillNode->IsNodeActivated()) {
                Tree->ActivateNode(SkillNode);
            } else if (SkillNode->IsNodeActivated() && !ActiveSkills.Contains(NodeId)) {
                Tree->DeactivateNode(SkillNode);
            }
        }
    }
}

bool UACFSkillTreeComponent::Internal_RemoveSkill(const FGameplayTag& SkillTreeTag, const FGuid& SkillNodeId)
{
    FAbilityHandlers* Handlers = SkillHandlers.Find(SkillNodeId);
    if (Handlers) {
        // Remove gameplay effect and clear ability
        UACFRPGFunctionLibrary::RemovesActiveGameplayEffectFromActor(Handlers->GEHandle, GetOwner());
        AbilityComp->ClearAbility(Handlers->GAHandle);
        SkillHandlers.Remove(SkillNodeId);

        // Remove from active skills container
        ActiveSkills.RemoveSkillById(SkillNodeId);

        BroadcastActiveSkillsChanged();
        return true;
    }
    return false;
}

void UACFSkillTreeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UACFSkillTreeComponent, ActiveSkills);
    DOREPLIFETIME(UACFSkillTreeComponent, SkillPoints);
}

void UACFSkillTreeComponent::UnlockSkillFromTree_Implementation(const FGameplayTag& SkillTreeTag, const FGuid& SkillNodeId)
{
    if (CanUnlockSkill(SkillTreeTag, SkillNodeId)) {
        UACFBaseSkillNode* SkillNode = GetSkillByIds(SkillTreeTag, SkillNodeId);
        if (SkillNode) {
            // Grant ability and apply gameplay effect
            const FGameplayAbilitySpecHandle AbilitySpec = AbilityComp->GrantAbility(SkillNode->GetSkillToGrant().AbilityToGrant);
            const FActiveGameplayEffectHandle EffectHandle = UACFRPGFunctionLibrary::AddGameplayEffectToActor(SkillNode->GetSkillToGrant().GameplayEffect, GetOwner());
            
            // Store handlers for this skill
            FAbilityHandlers Handlers(AbilitySpec, EffectHandle);
            SkillHandlers.Add(SkillNodeId, Handlers);
            
            // Deduct skill points and add skill to active list
            AddSkillPoints(-SkillNode->GetSkillToGrant().RequiredSkillPoint);
            ActiveSkills.AddSkill(FSkillSaveData(SkillNodeId, SkillTreeTag));
            
            BroadcastActiveSkillsChanged();
        }
    }
}

bool UACFSkillTreeComponent::CanUnlockSkill(const FGameplayTag& SkillTreeTag, const FGuid& SkillNodeId) const
{
    if (!AttributeComp) {
        UE_LOG(ACFLog, Error, TEXT("Invalid Attribute Component! - UACFSkillTreeComponent"));
        return false;
    }

    if (!SkillTreeTag.IsValid() || !SkillNodeId.IsValid()) {
        UE_LOG(ACFLog, Error, TEXT("Invalid SkillTreeTag or SkillNodeId! - UACFSkillTreeComponent"));
        return false;
    }

    const UACFSkillTreeGraph* SkillTree = GetSkillTreeByTag(SkillTreeTag);
    if (SkillTree) {
        const UACFBaseSkillNode* SkillNode = SkillTree->GetSkillNodeById(SkillNodeId);
        if (SkillNode) {
            // Check all unlock conditions
            const bool bAreParentsActive = SkillTree->AreAllParentsActive(SkillNodeId);
            const bool bHasEnoughSkillPoints = SkillNode->GetSkillToGrant().RequiredSkillPoint <= SkillPoints;
            const bool bMeetsLevelRequirement = AttributeComp->GetCurrentLevel() >= SkillNode->GetSkillToGrant().RequiredLevel;

            return bMeetsLevelRequirement && bAreParentsActive && bHasEnoughSkillPoints;
        }
    }
    return false;
}

void UACFSkillTreeComponent::AddSkillPoints_Implementation(int32 SkillPointsToAdd)
{
    SkillPoints += SkillPointsToAdd;
    BroadcastSkillPointsChanged();
}

UACFBaseSkillNode* UACFSkillTreeComponent::GetSkillByIds(const FGameplayTag& SkillTreeTag, const FGuid& SkillNodeId) const
{
    const UACFSkillTreeGraph* SkillTree = GetSkillTreeByTag(SkillTreeTag);
    if (SkillTree) {
        return SkillTree->GetSkillNodeById(SkillNodeId);
    }
    return nullptr;
}

UACFSkillTreeGraph* UACFSkillTreeComponent::GetSkillTreeByTag(const FGameplayTag& SkillTreeTag) const
{
    for (UACFSkillTreeGraph* Tree : SkillTrees) {
        if (Tree && Tree->GetSkillTreeTag() == SkillTreeTag) {
            return Tree;
        }
    }
    return nullptr;
}

bool UACFSkillTreeComponent::IsSkillActive(const FGuid& SkillNodeId) const
{
    return ActiveSkills.Contains(SkillNodeId);
}

void UACFSkillTreeComponent::RemoveSkill_Implementation(const FGameplayTag& SkillTreeTag, const FGuid& SkillNodeId)
{
    Internal_RemoveSkill(SkillTreeTag, SkillNodeId);
}

void UACFSkillTreeComponent::Respec_Implementation()
{
    TArray<FSkillSaveData> SkillsToRemove = ActiveSkills.Skills;
    int32 TotalSkillPoints = 0;
    
    // Calculate total skill points to refund and remove all skills
    for (const FSkillSaveData& SkillData : SkillsToRemove) {
        const UACFBaseSkillNode* SkillNode = GetSkillByIds(SkillData.SkillTreeTag, SkillData.SkillId);
        if (SkillNode) {
            TotalSkillPoints += SkillNode->GetSkillToGrant().RequiredSkillPoint;
        }
        Internal_RemoveSkill(SkillData.SkillTreeTag, SkillData.SkillId);
    }
    
    // Refund all skill points and clear containers
    AddSkillPoints(TotalSkillPoints);
    ActiveSkills.ClearAllSkills();
    SkillHandlers.Empty();

    BroadcastActiveSkillsChanged();
    BroadcastSkillPointsChanged();
}