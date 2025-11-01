// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/PangeaTamingComponent.h"

#include "Actors/ACFCharacter.h"
#include "DataAssets/TameSpeciesConfig.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"
#include "Blueprint/UserWidget.h"
#include "Components/ACFTeamComponent.h"
#include "UI/TamingWidget.h"

UPangeaTamingComponent::UPangeaTamingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPangeaTamingComponent::BeginPlay()
{
	Super::BeginPlay();
}

// ------------------------------------------------------------
// --------------------- TAME INITIALIZATION ------------------
// ------------------------------------------------------------

bool UPangeaTamingComponent::HasRequiredStats(AActor* Instigator) const
{
	if (!SpeciesConfig || SpeciesConfig->StatRequirements.IsEmpty()) 
		return true;

	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator);
	if (!ASC) return false;

	for (const FTameStatRequirement& Req : SpeciesConfig->StatRequirements)
	{
		if (!Req.Attribute.IsValid()) continue;

		const float Value = ASC->GetNumericAttribute(Req.Attribute);
		if (Value < Req.MinValue)
		{
			const FString Label = Req.DisplayTag.IsValid() ? Req.DisplayTag.ToString() : Req.Attribute.GetName();
			UE_LOG(LogTemp, Warning, TEXT("[Taming] Stat check failed %s: %.2f < %.2f"), *Label, Value, Req.MinValue);
			return false;
		}
	}
	return true;
}

bool UPangeaTamingComponent::HasRequiredItem(AActor* Instigator) const
{
	if (!SpeciesConfig || !SpeciesConfig->RequiredTamingItem) 
		return true;

	const UACFInventoryComponent* Inventory = Instigator->FindComponentByClass<UACFInventoryComponent>();
	return Inventory && Inventory->HasAnyItemOfType(SpeciesConfig->RequiredTamingItem) &&
	       Inventory->GetTotalCountOfItemsByClass(SpeciesConfig->RequiredTamingItem) >= SpeciesConfig->RequiredItemCount;
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

// ------------------------------------------------------------
// ---------------------- TAME ATTEMPT ------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::StartTameAttempt(AActor* Instigator)
{
	if (!SpeciesConfig) return;

	CachedInstigator = Instigator;

	AACFCharacter* Character = Cast<AACFCharacter>(Instigator);
	if (!Character) return;

	if (!HasRequiredItem(Instigator))
	{
		OnTameResolved(false, ETamedRole::None);
		return;
	}

	if (!HasRequiredStats(Instigator))
	{
		OnTameResolved(false, ETamedRole::None);
		return;
	}

	// Trigger the taming ability action
	if (SpeciesConfig->TameAbilityTag.IsValid())
	{
		Character->TriggerAction(SpeciesConfig->TameAbilityTag, EActionPriority::EHigh, false);
	}
}

// ------------------------------------------------------------
// ---------------------- TAME RESULTS ------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::OnTameResolved(bool bSuccess, ETamedRole DesiredRole)
{
	if (!SpeciesConfig) return;

	// Always clear the in-progress tag from player and target
	RemoveTagFromActor(CachedInstigator.Get(), SpeciesConfig->InProgressTag);
	RemoveTagFromActor(GetOwner(), SpeciesConfig->InProgressTag);

	// Apply success/fail tag to player
	AddTagToActor(CachedInstigator.Get(), bSuccess ? SpeciesConfig->SuccessTag : SpeciesConfig->FailTag);

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

	// --- Determine final role ---
	ETamedRole FinalRole = DesiredRole;
	if (FinalRole == ETamedRole::None)
	{
		if (SpeciesConfig->bCanBeMount && !SpeciesConfig->bCanBeCompanion)
			FinalRole = ETamedRole::Mount;
		else if (!SpeciesConfig->bCanBeMount && SpeciesConfig->bCanBeCompanion)
			FinalRole = ETamedRole::Companion;
		else if (SpeciesConfig->bCanBeMount && SpeciesConfig->bCanBeCompanion)
			FinalRole = ETamedRole::Mount; // Default fallback
	}

	SetTamedRole(FinalRole);
	GrantTamedAbilities();
	
	//Remove item from inventory
	FInventoryItem ItemToRemove;
	ItemToRemove.ItemClass = SpeciesConfig->RequiredTamingItem;
	CachedInstigator->FindComponentByClass<UACFInventoryComponent>()->RemoveItem(ItemToRemove, SpeciesConfig->RequiredItemCount);

	// --- Apply success effects ---
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

void UPangeaTamingComponent::HandleTameFailed(const FString& Reason, AActor* Instigator)
{
	if (!SpeciesConfig) return;

	// Apply failure tag
	AddTagToActor(Instigator, SpeciesConfig->FailTag);
	RemoveTagFromActor(Instigator, SpeciesConfig->InProgressTag);

	// Setup cooldown for retry
	if (UWorld* World = GetWorld())
	{
		const float CooldownTime = SpeciesConfig->RetryTameCooldown > 0 ? SpeciesConfig->RetryTameCooldown : 5.f;
		const FGameplayTag FailTag = SpeciesConfig->FailTag;

		World->GetTimerManager().SetTimerForNextTick([this, Instigator, CooldownTime, FailTag]()
		{
			if (UWorld* World = GetWorld())
			{
				FTimerHandle CooldownHandle;
				World->GetTimerManager().SetTimer(CooldownHandle, FTimerDelegate::CreateWeakLambda(this, [this, Instigator, FailTag]()
				{
					RemoveTagFromActor(Instigator, FailTag);
					UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Cooldown expired — %s can attempt taming again."), *GetNameSafe(Instigator));
				}), CooldownTime, false);
			}
		});
	}

	// Optional: Trigger runaway behavior
	if (SpeciesConfig->RunawayActionTag.IsValid())
	{
		if (AACFCharacter* Dino = Cast<AACFCharacter>(GetOwner()))
		{
			Dino->TriggerAction(SpeciesConfig->RunawayActionTag, EActionPriority::EHigh, false);
			UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Triggered runaway action '%s'"), *SpeciesConfig->RunawayActionTag.ToString());
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] %s: Tame failed — %s"), *GetNameSafe(GetOwner()), *Reason);
}

// ------------------------------------------------------------
// ---------------------- MINIGAME -----------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::OnMinigameResult(bool bSuccess)
{
	if (!SpeciesConfig)
		return;

	if (ActiveMinigameWidget)
	{
		ActiveMinigameWidget->RemoveFromParent();
		ActiveMinigameWidget = nullptr;
	}

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Minigame succeeded for %s"), *GetNameSafe(GetOwner()));
		OnTameResolved(true, TamedRole);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] Minigame failed for %s"), *GetNameSafe(GetOwner()));
		HandleTameFailed(TEXT("Minigame failed"), CachedInstigator.Get());
	}
}

void UPangeaTamingComponent::StartMinigame(AActor* Instigator, AActor* Target, float Duration)
{
	if (!MinigameWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] No MinigameWidgetClass assigned."));
		return;
	}
	
	APlayerController* PC = Cast<APlayerController>(Instigator ? Instigator->GetInstigatorController() : nullptr);
	if (!PC)
		PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;

	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] No valid PlayerController for CreateWidget."));
		return;
	}

	UTamingWidget* Widget = CreateWidget<UTamingWidget>(PC, MinigameWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] Failed to create TamingWidget instance."));
		return;
	}

	Widget->Duration = Duration;
	Widget->OnResult.AddDynamic(this, &UPangeaTamingComponent::OnMinigameResult);
	Widget->AddToViewport();
	ActiveMinigameWidget = Widget;

	UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Minigame started for %s"), *GetNameSafe(Target));
}

// ------------------------------------------------------------
// ---------------------- ROLE & STATE ------------------------
// ------------------------------------------------------------

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
				SwitchAIController(SpeciesConfig->TamedMountAIController);
			break;
		}
	case ETamedRole::Companion:
		{
			if (SpeciesConfig->TamedCompanionAIController)
				SwitchAIController(SpeciesConfig->TamedCompanionAIController);
			break;
		}
	default:
		ClearRoleTags();
		break;
	}
}

// ------------------------------------------------------------
// ---------------------- TAG MANAGEMENT ----------------------
// ------------------------------------------------------------

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
	case ETameState::Wild: AddTagToActor(GetOwner(), SpeciesConfig->WildStateTag); break;
	case ETameState::Hostile: AddTagToActor(GetOwner(), SpeciesConfig->HostileStateTag); break;
	case ETameState::Tamed: AddTagToActor(GetOwner(), SpeciesConfig->TamedStateTag); break;
	default: break;
	}
}

void UPangeaTamingComponent::ApplyRoleTag(ETamedRole Role)
{
	if (!SpeciesConfig) return;
	ClearRoleTags();
	if (Role == ETamedRole::Mount)
		AddTagToActor(GetOwner(), SpeciesConfig->MountRoleTag);
	else if (Role == ETamedRole::Companion)
		AddTagToActor(GetOwner(), SpeciesConfig->CompanionRoleTag);
}

// ------------------------------------------------------------
// ---------------------- UTILITY -----------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::ChangeTeam(const FGameplayTag& TeamTag) const
{
	if (UACFTeamComponent* TeamComp = GetOwner()->FindComponentByClass<UACFTeamComponent>())
	{
		if (TeamTag.IsValid())
			TeamComp->SetTeam(TeamTag);
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
				ASC->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE));
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

	if (AAIController* NewAIC = Pawn->GetWorld()->SpawnActor<AAIController>(NewControllerClass, Pawn->GetActorLocation(), Pawn->GetActorRotation()))
	{
		NewAIC->Possess(Pawn);
	}
}




