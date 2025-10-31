// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/PangeaTamingComponent.h"

#include "Actors/ACFCharacter.h"
#include "DataAssets/TameSpeciesConfig.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"
#include "Components/ACFTeamComponent.h"
#include "GameplayTasks/AbilityTask_WaitForTameResult.h"

UPangeaTamingComponent::UPangeaTamingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPangeaTamingComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UPangeaTamingComponent::HasRequiredStats(AActor* Instigator) const
{
	if (!SpeciesConfig || SpeciesConfig->StatRequirements.IsEmpty()) return true;

	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator);
	if (!ASC) return false;

	for (const FTameStatRequirement& Req : SpeciesConfig->StatRequirements)
	{
		if (!Req.Attribute.IsValid()) continue;

		const float Value = ASC->GetNumericAttribute(Req.Attribute);

		if (Value < Req.MinValue)
		{
			const FString Label = Req.DisplayTag.IsValid() ? Req.DisplayTag.ToString() : Req.Attribute.GetName();
			UE_LOG(LogTemp, Warning, TEXT("[Taming] Stat check failed %s: %.2f < %.2f"),
				*Label, Value, Req.MinValue);
			return false;
		}
	}
	return true;
}

bool UPangeaTamingComponent::HasRequiredItem(AActor* Instigator) const
{
	if (!SpeciesConfig || !SpeciesConfig->RequiredTamingItem) return true;

	const UACFInventoryComponent* Inventory = Instigator->FindComponentByClass<UACFInventoryComponent>();
	return Inventory && Inventory->HasAnyItemOfType(SpeciesConfig->RequiredTamingItem) && Inventory->GetTotalCountOfItemsByClass(SpeciesConfig->RequiredTamingItem) >= SpeciesConfig->RequiredItemCount;
}

void UPangeaTamingComponent::InitializeWild()
{
	if (!SpeciesConfig) return;
	
	ClearStateTags();
	TameState = ETameState::Wild;
	ApplyStateTags();
	ChangeTeam(SpeciesConfig->WildTeamTag);
	OnTameStateChanged.Broadcast(TameState);
}

void UPangeaTamingComponent::InitializeHostile()
{
	if (!SpeciesConfig) return;
	
	ClearStateTags();
	TameState = ETameState::Hostile;
	ApplyStateTags();
	ChangeTeam(SpeciesConfig->HostileTeamTag);
	OnTameStateChanged.Broadcast(TameState);
}

void UPangeaTamingComponent::StartTameAttempt(AActor* Instigator)
{
	if (!SpeciesConfig) return;

	AACFCharacter* Character = Cast<AACFCharacter>(Instigator);
	if (!Character) return;

	if (!HasRequiredItem(Instigator))
	{
		//HandleTameFailed(TEXT("Missing required item."));
		OnTameResolved(false, ETamedRole::None);
		return;
	}

	if (!HasRequiredStats(Instigator))
	{
		//HandleTameFailed(TEXT("Insufficient stats."));
		OnTameResolved(false, ETamedRole::None);
		return;
	}

	AddTagToActor(Instigator, SpeciesConfig->InProgressTag);

	// Trigger action
	if (SpeciesConfig->TameAbilityTag.IsValid())
	{
		Character->TriggerAction(SpeciesConfig->TameAbilityTag, EActionPriority::EHigh, false);
	}
}

void UPangeaTamingComponent::AbortTameAttempt()
{
	if (!SpeciesConfig) return;
	
	RemoveTagFromActor(GetOwner(), SpeciesConfig->InProgressTag);
}

void UPangeaTamingComponent::OnTameResolved(bool bSuccess, ETamedRole DesiredRole)
{
	if (!SpeciesConfig) return;

	RemoveTagFromActor(GetOwner(), SpeciesConfig->InProgressTag);
	AddTagToActor(GetOwner(), bSuccess ? SpeciesConfig->SuccessTag : SpeciesConfig->FailTag);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Tame failed for %s"), *GetNameSafe(GetOwner()));
		return;
	}

	// --- Update tame state ---
	ClearStateTags();
	TameState = ETameState::Tamed;
	ApplyStateTags();
	ChangeTeam(SpeciesConfig->TamedTeamTag);

	// --- Determine role automatically if not explicitly specified ---
	ETamedRole FinalRole = DesiredRole;

	if (FinalRole == ETamedRole::None)
	{
		if (SpeciesConfig->bCanBeMount && !SpeciesConfig->bCanBeCompanion)
			FinalRole = ETamedRole::Mount;
		else if (!SpeciesConfig->bCanBeMount && SpeciesConfig->bCanBeCompanion)
			FinalRole = ETamedRole::Companion;
		else if (SpeciesConfig->bCanBeMount && SpeciesConfig->bCanBeCompanion)
			FinalRole = ETamedRole::Mount; // default; could expose UI later
	}

	SetTamedRole(FinalRole);

	// --- Apply success rewards and effects ---
	GrantTamedAbilities();

	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		for (const TSubclassOf<UGameplayEffect>& GEClass : SpeciesConfig->EffectsOnTameSuccess)
		{
			if (GEClass)
			{
				ASC->ApplyGameplayEffectToSelf(GEClass->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[TamingComponent] %s successfully tamed as %s"),
		*GetNameSafe(GetOwner()),
		FinalRole == ETamedRole::Mount ? TEXT("Mount") :
		(FinalRole == ETamedRole::Companion ? TEXT("Companion") : TEXT("Unknown Role")));
}

void UPangeaTamingComponent::HandleTameFailed(const FString& Reason)
{
	if (!SpeciesConfig) return;

	RemoveTagFromActor(GetOwner(), SpeciesConfig->InProgressTag);
	AddTagToActor(GetOwner(), SpeciesConfig->FailTag);

	UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] %s: Tame failed — %s"),
		*GetNameSafe(GetOwner()), *Reason);

	OnTameFailed.Broadcast(Reason);

	// Trigger runaway action if configured
	if (SpeciesConfig->RunawayActionTag.IsValid())
	{
		if (AACFCharacter* Dino = Cast<AACFCharacter>(GetOwner()))
		{
			Dino->TriggerAction(SpeciesConfig->RunawayActionTag, EActionPriority::EHigh, false);
			UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Triggered runaway action '%s'"),
				*SpeciesConfig->RunawayActionTag.ToString());
		}
	}
}


void UPangeaTamingComponent::SetTamedRole(ETamedRole NewRole)
{
	if (!SpeciesConfig) return;

	TamedRole = NewRole;
	ApplyRoleTag(NewRole);
	OnTameRoleSelected.Broadcast(TamedRole);

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;

	switch (NewRole)
	{
	case ETamedRole::Mount:
		{
			if (AController* Old = Pawn->GetController())
			{
				Old->UnPossess();
				Old->Destroy();
			}

			if (SpeciesConfig->TamedMountAIController)
			{
				SwitchAIController(SpeciesConfig->TamedMountAIController);
			}

			// Player will possess this pawn via interaction
			break;
		}
	case ETamedRole::Companion:
		{
			if (SpeciesConfig->TamedCompanionAIController)
			{
				SwitchAIController(SpeciesConfig->TamedCompanionAIController);
			}
			break;
		}
	default:
		ClearRoleTags();
		break;
	}
}

void UPangeaTamingComponent::BeginTameMinigame(UGameplayAbility* OwningAbility, UAbilityTask_WaitForTameResult* WaitTask)
{
	if (!OwningAbility || !WaitTask)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] BeginTameMinigame called with invalid parameters."));
		return;
	}

	CachedTameTask = WaitTask;
	AActor* Instigator = OwningAbility->GetAvatarActorFromActorInfo();

	float Duration = SpeciesConfig ? SpeciesConfig->TameDuration : 5.f;

	UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Opening minigame for %s (target: %s)"),
		*GetNameSafe(Instigator),
		*GetNameSafe(GetOwner()));

	// Open the Blueprint minigame UI.
	OpenTamingMinigame(Instigator, GetOwner(), Duration);
}

void UPangeaTamingComponent::OnMinigameResult(bool bSuccess)
{
	if (!CachedTameTask)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] OnMinigameResult called but no cached task exists!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Minigame completed (%s) — notifying task."),
		bSuccess ? TEXT("SUCCESS") : TEXT("FAILURE"));

	CachedTameTask->NotifyTameResult(bSuccess);
	CachedTameTask = nullptr;
}

void UPangeaTamingComponent::ClearStateTags()
{
	if (!SpeciesConfig) return;

	RemoveTagFromActor(GetOwner(), SpeciesConfig->WildStateTag);
	RemoveTagFromActor(GetOwner(), SpeciesConfig->HostileStateTag);
	RemoveTagFromActor(GetOwner(), SpeciesConfig->TamedStateTag);
}

void UPangeaTamingComponent::ClearRoleTags()
{
	if (!SpeciesConfig) return;
	
	RemoveTagFromActor(GetOwner(), SpeciesConfig->MountRoleTag);
	RemoveTagFromActor(GetOwner(), SpeciesConfig->CompanionRoleTag);
}

void UPangeaTamingComponent::ApplyStateTags()
{
	if (!SpeciesConfig) return;
	
	switch (TameState)
	{
	case ETameState::Wild:
		AddTagToActor(GetOwner(), SpeciesConfig->WildStateTag);
		break;
	case ETameState::Hostile:
		AddTagToActor(GetOwner(), SpeciesConfig->HostileStateTag);
		break;
	case ETameState::Tamed:
		AddTagToActor(GetOwner(), SpeciesConfig->TamedStateTag);
		break;
	default: break;
	}
}

void UPangeaTamingComponent::ChangeTeam(const FGameplayTag& TeamTag) const
{
	UACFTeamComponent* TeamComp = GetOwner()->FindComponentByClass<UACFTeamComponent>();

	if (!TeamComp) return;

	if (TeamTag.IsValid())
	{
		TeamComp->SetTeam(TeamTag);
	}
}

void UPangeaTamingComponent::ApplyRoleTag(ETamedRole Role)
{
	if (!SpeciesConfig) return;

	ClearRoleTags();

	if (Role == ETamedRole::Mount)
	{
		AddTagToActor(GetOwner(), SpeciesConfig->MountRoleTag);
	}
	else if (Role == ETamedRole::Companion)
	{
		AddTagToActor(GetOwner(), SpeciesConfig->CompanionRoleTag);
	}
}

void UPangeaTamingComponent::GrantTamedAbilities() const
{
	if (!SpeciesConfig) return;
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		for (const TSubclassOf<UGameplayAbility>& Ability : SpeciesConfig->AbilitiesGrantedWhenTamed)
		{
			if (Ability)
			{
				ASC->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE));
			}
		}
	}
}

void UPangeaTamingComponent::SwitchAIController(TSubclassOf<AAIController> NewControllerClass) const
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !NewControllerClass) return;

	if (AController* Old = Pawn->GetController())
	{
		Old->UnPossess();
		Old->Destroy();
	}

	AAIController* NewAIC = Pawn->GetWorld()->SpawnActor<AAIController>(NewControllerClass, Pawn->GetActorLocation(), Pawn->GetActorRotation());
	if (NewAIC)
	{
		NewAIC->Possess(Pawn);
	}
}




