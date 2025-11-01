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
// --------------------- PREREQUISITES ------------------------
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

bool UPangeaTamingComponent::CheckTamePrerequisites(AActor* Instigator) const
{
	return HasRequiredItem(Instigator) && HasRequiredStats(Instigator);
}

// ------------------------------------------------------------
// --------------------- INITIALIZATION -----------------------
// ------------------------------------------------------------

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
// --------------------- TAME ATTEMPT -------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::StartTameAttempt(AActor* Instigator)
{
	if (!SpeciesConfig) return;
	CachedInstigator = Instigator;

	if (!CheckTamePrerequisites(Instigator))
	{
		OnTameResolved(false, ETamedRole::None);
		return;
	}

	if (AACFCharacter* Character = Cast<AACFCharacter>(Instigator))
	{
		if (SpeciesConfig->TameAbilityTag.IsValid())
			Character->TriggerAction(SpeciesConfig->TameAbilityTag, EActionPriority::EHigh, false);
	}
}

// ------------------------------------------------------------
// --------------------- TAME RESULTS -------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::OnTameResolved(bool bSuccess, ETamedRole DesiredRole)
{
	if (!SpeciesConfig) return;

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
	if (!SpeciesConfig) return;

	// --- Update tame state ---
	ClearStateTags();
	TameState = ETameState::Tamed;
	ApplyStateTags();
	ChangeTeam(SpeciesConfig->TamedTeamTag);

	// --- Determine and apply final role ---
	ETamedRole FinalRole = DetermineFinalRole(DesiredRole);
	SetTamedRole(FinalRole);

	// --- Apply abilities, items, and effects ---
	GrantTamedAbilities();
	ApplyTameSuccessEffects();
	ConsumeTameItems();

	UE_LOG(LogTemp, Log, TEXT("[TamingComponent] %s transitioned to Tamed (Role: %s)"),
		*GetNameSafe(GetOwner()),
		FinalRole == ETamedRole::Mount ? TEXT("Mount") :
		(FinalRole == ETamedRole::Companion ? TEXT("Companion") : TEXT("Unknown")));
}

void UPangeaTamingComponent::TransitionToFailedState(AActor* Instigator)
{
	if (!SpeciesConfig || !Instigator) return;

	AddTagToActor(Instigator, SpeciesConfig->FailTag);
	StartTameCooldown(Instigator);

	if (SpeciesConfig->RunawayActionTag.IsValid())
	{
		if (AACFCharacter* Dino = Cast<AACFCharacter>(GetOwner()))
			Dino->TriggerAction(SpeciesConfig->RunawayActionTag, EActionPriority::EHigh, false);
	}

	UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] %s: Tame failed."), *GetNameSafe(GetOwner()));
}

// ------------------------------------------------------------
// --------------------- COOLDOWN -----------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::StartTameCooldown(AActor* Instigator)
{
	if (!SpeciesConfig || !Instigator) return;

	const float Cooldown = FMath::Max(0.1f, SpeciesConfig->RetryTameCooldown);
	const FGameplayTag FailTag = SpeciesConfig->FailTag;

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
	if (!SpeciesConfig) return;

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
	if (!SpeciesConfig) return;

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
			if (SpeciesConfig->TamedMountAIController)
				SwitchAIController(SpeciesConfig->TamedMountAIController);
			break;

		case ETamedRole::Companion:
			if (SpeciesConfig->TamedCompanionAIController)
				SwitchAIController(SpeciesConfig->TamedCompanionAIController);
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

	if (!SpeciesConfig)
		return ETamedRole::None;

	if (SpeciesConfig->bCanBeMount && !SpeciesConfig->bCanBeCompanion)
		return ETamedRole::Mount;

	if (!SpeciesConfig->bCanBeMount && SpeciesConfig->bCanBeCompanion)
		return ETamedRole::Companion;

	if (SpeciesConfig->bCanBeMount && SpeciesConfig->bCanBeCompanion)
		return ETamedRole::Mount; // default fallback

	return ETamedRole::None;
}

// ------------------------------------------------------------
// --------------------- TAG MANAGEMENT -----------------------
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

void UPangeaTamingComponent::ClearTameTags()
{
	if (!SpeciesConfig) return;
	RemoveTagFromActor(CachedInstigator.Get(), SpeciesConfig->InProgressTag);
	RemoveTagFromActor(GetOwner(), SpeciesConfig->InProgressTag);
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
// --------------------- EFFECTS & ITEMS ----------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::ApplyTameSuccessEffects() const
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		for (const TSubclassOf<UGameplayEffect>& GEClass : SpeciesConfig->EffectsOnTameSuccess)
			if (GEClass)
				ASC->ApplyGameplayEffectToSelf(GEClass->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
	}
}

void UPangeaTamingComponent::ConsumeTameItems() const
{
	if (!SpeciesConfig || !CachedInstigator.IsValid()) return;

	if (UACFInventoryComponent* Inv = CachedInstigator->FindComponentByClass<UACFInventoryComponent>())
	{
		FInventoryItem Item;
		Item.ItemClass = SpeciesConfig->RequiredTamingItem;
		Inv->RemoveItem(Item, SpeciesConfig->RequiredItemCount);
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
	if (!SpeciesConfig) return;
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		for (const TSubclassOf<UGameplayAbility>& Ability : SpeciesConfig->AbilitiesGrantedWhenTamed)
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




