// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/PangeaTamingComponent.h"

#include "Actors/ACFCharacter.h"
#include "DataAssets/TameSpeciesConfig.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"
#include "Blueprint/UserWidget.h"
#include "Components/ACFCompanionGroupAIComponent.h"
#include "Components/ACFInteractionComponent.h"
#include "Components/ACFTeamComponent.h"
#include "Game/ACFFunctionLibrary.h"
#include "Game/ACFPlayerController.h"
#include "UI/TamingWidget.h"

UPangeaTamingComponent::UPangeaTamingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPangeaTamingComponent::BeginPlay()
{
	Super::BeginPlay();

	//bind event
	OnTameStateChanged.AddDynamic(this, &UPangeaTamingComponent::HandleTameStateChanged);
}

// ------------------------------------------------------------
// --------------------- PREREQUISITES ------------------------
// ------------------------------------------------------------

bool UPangeaTamingComponent::HasRequiredStats(AActor* Instigator) const
{
	if (!TameSpeciesConfig || TameSpeciesConfig->StatRequirements.IsEmpty())
		return true;

	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator);
	if (!ASC) return false;

	for (const FTameStatRequirement& Req : TameSpeciesConfig->StatRequirements)
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
	if (!TameSpeciesConfig || !TameSpeciesConfig->RequiredTamingItem)
		return true;

	const UACFInventoryComponent* Inventory = Instigator->FindComponentByClass<UACFInventoryComponent>();
	return Inventory && Inventory->HasAnyItemOfType(TameSpeciesConfig->RequiredTamingItem) &&
	       Inventory->GetTotalCountOfItemsByClass(TameSpeciesConfig->RequiredTamingItem) >= TameSpeciesConfig->RequiredItemCount;
}

bool UPangeaTamingComponent::CheckTamePrerequisites(AActor* Instigator) const
{
	return HasRequiredItem(Instigator) && HasRequiredStats(Instigator);
}

// ------------------------------------------------------------
// --------------------- INITIALIZATION -----------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::InitializeWild()
{
	if (!TameSpeciesConfig) return;
	ClearStateTags();
	TameState = ETameState::Wild;
	ApplyStateTags();
	ChangeTeam(TameSpeciesConfig->WildTeamTag);
	OnTameStateChanged.Broadcast(TameState);
}

void UPangeaTamingComponent::InitializeHostile()
{
	if (!TameSpeciesConfig) return;
	ClearStateTags();
	TameState = ETameState::Hostile;
	ApplyStateTags();
	ChangeTeam(TameSpeciesConfig->HostileTeamTag);
	OnTameStateChanged.Broadcast(TameState);
}

// ------------------------------------------------------------
// --------------------- TAME ATTEMPT -------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::StartTameAttempt(AActor* Instigator)
{
	if (!TameSpeciesConfig) return;
	CachedInstigator = Instigator;

	if (!CheckTamePrerequisites(Instigator))
	{
		OnTameResolved(false, ETamedRole::None);
		return;
	}

	if (AACFCharacter* Character = Cast<AACFCharacter>(Instigator))
	{
		if (TameSpeciesConfig->TameAbilityTag.IsValid())
			Character->TriggerAction(TameSpeciesConfig->TameAbilityTag, EActionPriority::EHigh, false);
	}
}

// ------------------------------------------------------------
// --------------------- TAME RESULTS -------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::OnTameResolved(bool bSuccess, ETamedRole DesiredRole)
{
	if (!TameSpeciesConfig) return;

	ClearTameTags();

	if (!bSuccess)
	{
		TransitionToFailedState(CachedInstigator.Get());
		return;
	}

	TransitionToTamedState(DesiredRole);
}

void UPangeaTamingComponent::TransitionToTamedState(ETamedRole DesiredRole)
{
	if (!TameSpeciesConfig) return;

	// --- Update tame state ---
	ClearStateTags();
	TameState = ETameState::Tamed;
	ApplyStateTags();
	ChangeTeam(TameSpeciesConfig->TamedTeamTag);

	// --- Determine and apply final role ---
	ETamedRole FinalRole = DetermineFinalRole(DesiredRole);
	SetTamedRole(FinalRole);

	// --- Apply abilities, items, and effects ---
	GrantTamedAbilities();
	ApplyTameSuccessEffects();
	ConsumeTameItems();

	// --- Broadcast event ---
	OnTameStateChanged.Broadcast(TameState);

	UE_LOG(LogTemp, Log, TEXT("[TamingComponent] %s transitioned to Tamed (Role: %s)"),
		*GetNameSafe(GetOwner()),
		FinalRole == ETamedRole::Mount ? TEXT("Mount") :
		(FinalRole == ETamedRole::Companion ? TEXT("Companion") : TEXT("Unknown")));
}

void UPangeaTamingComponent::TransitionToFailedState(AActor* Instigator)
{
	if (!TameSpeciesConfig || !Instigator) return;

	AddTagToActor(Instigator, TameSpeciesConfig->FailTag);
	StartTameCooldown(Instigator);

	if (TameSpeciesConfig->RunawayActionTag.IsValid())
	{
		if (AACFCharacter* Dino = Cast<AACFCharacter>(GetOwner()))
			Dino->TriggerAction(TameSpeciesConfig->RunawayActionTag, EActionPriority::EHigh, false);
	}

	UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] %s: Tame failed."), *GetNameSafe(GetOwner()));
}

void UPangeaTamingComponent::HandleTameStateChanged(ETameState NewState)
{
	UACFFunctionLibrary::GetLocalACFPlayerCharacter(GetOwner())->GetComponentByClass<UACFInteractionComponent>()->UnregisterInteractable(GetOwner());

	if (TamedRole == ETamedRole::Companion)
	{
		UACFFunctionLibrary::GetLocalACFPlayerController(GetWorld())->GetComponentByClass<UACFCompanionGroupAIComponent>()->AddExistingCharacterToGroup(Cast<AACFCharacter>(GetOwner()));
	}
	
	UACFFunctionLibrary::GetLocalACFPlayerCharacter(GetOwner())->GetComponentByClass<UACFInteractionComponent>()->RegisterInteractable(GetOwner());
}

// ------------------------------------------------------------
// --------------------- COOLDOWN -----------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::StartTameCooldown(AActor* Instigator)
{
	if (!TameSpeciesConfig || !Instigator) return;

	const float Cooldown = FMath::Max(0.1f, TameSpeciesConfig->RetryTameCooldown);
	const FGameplayTag FailTag = TameSpeciesConfig->FailTag;

	GetWorld()->GetTimerManager().SetTimerForNextTick([this, Instigator, FailTag, Cooldown]()
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(
				Handle,
				FTimerDelegate::CreateWeakLambda(this, [this, Instigator, FailTag]()
				{
					RemoveTagFromActor(Instigator, FailTag);
					UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Cooldown expired for %s"), *GetNameSafe(Instigator));
				}),
				Cooldown, false);
		}
	});
}

// ------------------------------------------------------------
// --------------------- MINIGAME -----------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::StartMinigame(AActor* Instigator, AActor* Target, float Duration)
{
	if (!MinigameWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] No MinigameWidgetClass assigned."));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Instigator ? Instigator->GetInstigatorController() : nullptr);
	if (!PC && GetWorld())
		PC = GetWorld()->GetFirstPlayerController();

	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] No valid PlayerController for CreateWidget."));
		return;
	}

	if (UTamingWidget* Widget = CreateWidget<UTamingWidget>(PC, MinigameWidgetClass))
	{
		Widget->Duration = Duration;
		Widget->OnResult.AddDynamic(this, &UPangeaTamingComponent::OnMinigameResult);
		Widget->AddToViewport();
		ActiveMinigameWidget = Widget;
		UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Minigame started for %s"), *GetNameSafe(Target));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] Failed to create TamingWidget instance."));
	}
}

void UPangeaTamingComponent::OnMinigameResult(bool bSuccess)
{
	if (!TameSpeciesConfig) return;

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
		OnTameResolved(false, TamedRole);
	}
}

// ------------------------------------------------------------
// --------------------- STATE & ROLE -------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::SetTamedRole(ETamedRole NewRole)
{
	if (!TameSpeciesConfig) return;

	TamedRole = NewRole;
	ApplyRoleTag(NewRole);
	OnTameRoleSelected.Broadcast(TamedRole);

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		switch (NewRole)
		{
		case ETamedRole::Mount:
			if (AController* Old = Pawn->GetController())
			{
				Old->UnPossess();
				Old->Destroy();
			}
			if (TameSpeciesConfig->TamedMountAIController)
				SwitchAIController(TameSpeciesConfig->TamedMountAIController);
			break;

		case ETamedRole::Companion:
			if (TameSpeciesConfig->TamedCompanionAIController)
				SwitchAIController(TameSpeciesConfig->TamedCompanionAIController);
			break;

		default:
			ClearRoleTags();
			break;
		}
	}
}

ETamedRole UPangeaTamingComponent::DetermineFinalRole(ETamedRole DesiredRole) const
{
	if (DesiredRole != ETamedRole::None)
		return DesiredRole;

	if (!TameSpeciesConfig)
		return ETamedRole::None;

	if (TameSpeciesConfig->bCanBeMount && !TameSpeciesConfig->bCanBeCompanion)
		return ETamedRole::Mount;

	if (!TameSpeciesConfig->bCanBeMount && TameSpeciesConfig->bCanBeCompanion)
		return ETamedRole::Companion;

	if (TameSpeciesConfig->bCanBeMount && TameSpeciesConfig->bCanBeCompanion)
		return ETamedRole::Mount; // default fallback

	return ETamedRole::None;
}

// ------------------------------------------------------------
// --------------------- TAG MANAGEMENT -----------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::ClearStateTags()
{
	if (!TameSpeciesConfig) return;
	RemoveTagFromActor(GetOwner(), TameSpeciesConfig->WildStateTag);
	RemoveTagFromActor(GetOwner(), TameSpeciesConfig->HostileStateTag);
	RemoveTagFromActor(GetOwner(), TameSpeciesConfig->TamedStateTag);
}

void UPangeaTamingComponent::ClearRoleTags()
{
	if (!TameSpeciesConfig) return;
	RemoveTagFromActor(GetOwner(), TameSpeciesConfig->MountRoleTag);
	RemoveTagFromActor(GetOwner(), TameSpeciesConfig->CompanionRoleTag);
}

void UPangeaTamingComponent::ClearTameTags()
{
	if (!TameSpeciesConfig) return;
	RemoveTagFromActor(CachedInstigator.Get(), TameSpeciesConfig->InProgressTag);
	RemoveTagFromActor(GetOwner(), TameSpeciesConfig->InProgressTag);
}

void UPangeaTamingComponent::ApplyStateTags()
{
	if (!TameSpeciesConfig) return;
	switch (TameState)
	{
	case ETameState::Wild: AddTagToActor(GetOwner(), TameSpeciesConfig->WildStateTag); break;
	case ETameState::Hostile: AddTagToActor(GetOwner(), TameSpeciesConfig->HostileStateTag); break;
	case ETameState::Tamed: AddTagToActor(GetOwner(), TameSpeciesConfig->TamedStateTag); break;
	default: break;
	}
}

void UPangeaTamingComponent::ApplyRoleTag(ETamedRole Role)
{
	if (!TameSpeciesConfig) return;
	ClearRoleTags();

	if (Role == ETamedRole::Mount)
		AddTagToActor(GetOwner(), TameSpeciesConfig->MountRoleTag);
	else if (Role == ETamedRole::Companion)
		AddTagToActor(GetOwner(), TameSpeciesConfig->CompanionRoleTag);
}

// ------------------------------------------------------------
// --------------------- EFFECTS & ITEMS ----------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::ApplyTameSuccessEffects() const
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		for (const TSubclassOf<UGameplayEffect>& GEClass : TameSpeciesConfig->EffectsOnTameSuccess)
			if (GEClass)
				ASC->ApplyGameplayEffectToSelf(GEClass->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
	}
}

void UPangeaTamingComponent::ConsumeTameItems() const
{
	if (!TameSpeciesConfig || !CachedInstigator.IsValid()) return;

	if (UACFInventoryComponent* Inv = CachedInstigator->FindComponentByClass<UACFInventoryComponent>())
	{
		FInventoryItem Item;
		Item.ItemClass = TameSpeciesConfig->RequiredTamingItem;
		Inv->RemoveItem(Item, TameSpeciesConfig->RequiredItemCount);
	}
}

// ------------------------------------------------------------
// --------------------- UTILITIES ----------------------------
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
	if (!TameSpeciesConfig) return;
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		for (const TSubclassOf<UGameplayAbility>& Ability : TameSpeciesConfig->AbilitiesGrantedWhenTamed)
			if (Ability)
				ASC->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE));
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
		NewAIC->Possess(Pawn);
}




