// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/PangeaTamingComponent.h"
#include "ANSUIPlayerSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonActivatableWidget.h"
#include "Engine/GameInstance.h"

#include "Actors/ACFCharacter.h"
#include "DataAssets/TameSpeciesConfig.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "Definitions/PangeaTamingFragment.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"
#include "Blueprint/UserWidget.h"
#include "Components/ACFCompanionGroupAIComponent.h"
#include "Components/ACFInteractionComponent.h"
#include "Components/ACFInventoryComponent.h"
#include "Components/ACFTeamComponent.h"
#include "Game/ACFFunctionLibrary.h"
#include "Game/ACFPlayerController.h"
#include "Interfaces/PDDefinitionProviderInterface.h"
#include "UI/TamingWidget.h"

UPangeaTamingComponent::UPangeaTamingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPangeaTamingComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveConfigFromDefinition();
	ResolveConfigFromOwnerClass();

	OnTameStateChanged.AddUniqueDynamic(this, &UPangeaTamingComponent::HandleTameStateChanged);
	InitializeStartupTameState();
}

void UPangeaTamingComponent::ResolveConfigFromDefinition()
{
	if (TameSpeciesConfig)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetClass()->ImplementsInterface(UPDDefinitionProviderInterface::StaticClass()))
	{
		return;
	}

	UPangeaCreatureDefinition* Definition = IPDDefinitionProviderInterface::Execute_GetCreatureDefinition(Owner);
	if (!Definition)
	{
		return;
	}

	if (const UPangeaTamingFragment* TamingFragment = Definition->GetFragment<UPangeaTamingFragment>())
	{
		TameSpeciesConfig = TamingFragment->TameSpeciesConfig;
	}
}

void UPangeaTamingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTameTags();
	if (UTamingWidget* Widget = Cast<UTamingWidget>(ActiveMinigameWidget))
	{
		Widget->OnResult.RemoveDynamic(this, &UPangeaTamingComponent::OnMinigameResult);
		if (GetWorld() && GetWorld()->GetGameInstance())
            if (UANSUIPlayerSubsystem* UI = GetWorld()->GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>()) UI->RemoveInGameWidget(Widget, false, false);
	}
	ActiveMinigameWidget = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UPangeaTamingComponent::ResolveConfigFromOwnerClass()
{
	if (TameSpeciesConfig || OwnerClassConfigMap.IsEmpty())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	for (UClass* CurrentClass = Owner->GetClass(); CurrentClass && CurrentClass != AActor::StaticClass(); CurrentClass = CurrentClass->GetSuperClass())
	{
		for (const TPair<TSoftClassPtr<AActor>, TObjectPtr<UTameSpeciesConfig>>& Entry : OwnerClassConfigMap)
		{
			UClass* MappedClass = Entry.Key.LoadSynchronous();
			if (MappedClass == CurrentClass && Entry.Value)
			{
				TameSpeciesConfig = Entry.Value;
				return;
			}
		}
	}
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
	if (!IsValid(Instigator)) return false;

	if (!TameSpeciesConfig || !TameSpeciesConfig->RequiredTamingItem)
		return true;

	const UACFInventoryComponent* Inventory = Instigator->FindComponentByClass<UACFInventoryComponent>();
	return Inventory && Inventory->HasAnyItemOfType(TameSpeciesConfig->RequiredTamingItem) &&
	       Inventory->GetTotalCountOfItemsByClass(TameSpeciesConfig->RequiredTamingItem) >= TameSpeciesConfig->RequiredItemCount;
}

bool UPangeaTamingComponent::CheckTamePrerequisites(AActor* Instigator) const
{
	if (!IsValid(Instigator) || !CanBeTamed() || TamedState == ETameState::Tamed)
		return false;
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator);
	if (ASC && TameSpeciesConfig->FailTag.IsValid() && ASC->HasMatchingGameplayTag(TameSpeciesConfig->FailTag))
		return false;
	return HasRequiredItem(Instigator) && HasRequiredStats(Instigator);
}

// ------------------------------------------------------------
// --------------------- INITIALIZATION -----------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::InitializeStartupTameState()
{
	if (!TameSpeciesConfig) return;

	if (TamedState == ETameState::Wild || TamedState == ETameState::Hostile || TamedState == ETameState::Tamed)
	{
		ClearStateTags();
		ApplyStateTags();
		OnTameStateChanged.Broadcast(TamedState);
	}
}

void UPangeaTamingComponent::InitializeWild()
{
	if (!TameSpeciesConfig) return;
	ClearStateTags();
	TamedState = ETameState::Wild;
	ApplyStateTags();
	ChangeTeam(TameSpeciesConfig->WildTeamTag);
	OnTameStateChanged.Broadcast(TamedState);
}

void UPangeaTamingComponent::InitializeHostile()
{
	if (!TameSpeciesConfig) return;
	ClearStateTags();
	TamedState = ETameState::Hostile;
	ApplyStateTags();
	ChangeTeam(TameSpeciesConfig->HostileTeamTag);
	OnTameStateChanged.Broadcast(TamedState);
}

// ------------------------------------------------------------
// --------------------- TAME ATTEMPT -------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::StartTameAttempt(AActor* Instigator)
{
	if (!CheckTamePrerequisites(Instigator) || IsValid(ActiveMinigameWidget)) return;
	if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator))
	{
		if (TameSpeciesConfig->InProgressTag.IsValid() && ASC->HasMatchingGameplayTag(TameSpeciesConfig->InProgressTag)) return;
	}
	CachedInstigator = Instigator;

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
	if (!TameSpeciesConfig || TamedState == ETameState::Tamed) return;
	// Recheck the item at completion; it may have been spent during the minigame.
	bSuccess = bSuccess && CachedInstigator.IsValid() && HasRequiredItem(CachedInstigator.Get());

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
	TamedState = ETameState::Tamed;
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
	OnTameStateChanged.Broadcast(TamedState);

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
	const APawn* Tamer = Cast<APawn>(CachedInstigator.Get());
	APlayerController* Controller = Tamer ? Cast<APlayerController>(Tamer->GetController()) : nullptr;
	// Restoring an existing single-player save has no current taming instigator.
	if (!Controller && GetNetMode() == NM_Standalone)
		Controller = UACFFunctionLibrary::GetLocalACFPlayerController(GetWorld());
	if (NewState == ETameState::Tamed && TamedRole == ETamedRole::Companion && Controller && GetOwner()->HasAuthority())
	{
		if (UACFCompanionGroupAIComponent* Group = Controller->FindComponentByClass<UACFCompanionGroupAIComponent>())
			Group->AddExistingCharacterToGroup(Cast<AACFCharacter>(GetOwner()));
	}
	// Do not forcibly register distant dinosaurs or steal another player's interaction.
	if (Controller && Controller->GetPawn())
	{
		if (UACFInteractionComponent* Interaction = Controller->GetPawn()->FindComponentByClass<UACFInteractionComponent>())
			Interaction->RefreshInteractions();
	}
}

void UPangeaTamingComponent::HandleLoadedActor()
{
	ResolveConfigFromDefinition();
	ResolveConfigFromOwnerClass();
	if (GetTameState() == ETameState::Tamed)
	{
		// Restoration is not a second tame: never consume items or stack success effects.
		ClearStateTags();
		ApplyStateTags();
		if (TameSpeciesConfig)
			ChangeTeam(TameSpeciesConfig->TamedTeamTag);
		SetTamedRole(GetTamedRole());
		GrantTamedAbilities();
		OnTameStateChanged.Broadcast(TamedState);
	}
	else
		InitializeStartupTameState();
}

// ------------------------------------------------------------
// --------------------- COOLDOWN -----------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::StartTameCooldown(AActor* Instigator)
{
	if (!TameSpeciesConfig || !Instigator) return;

	const float Cooldown = FMath::Max(0.1f, TameSpeciesConfig->RetryTameCooldown);
	const FGameplayTag FailTag = TameSpeciesConfig->FailTag;

	// The tag belongs to the instigator; its cleanup must survive this dinosaur's despawn.
	if (UWorld* World = GetWorld())
	{
		FTimerHandle Handle;
		const TWeakObjectPtr<AActor> WeakInstigator(Instigator);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Instigator, [WeakInstigator, FailTag]()
		{
			if (AActor* Actor = WeakInstigator.Get())
			{
				FGameplayTagContainer Tags;
				if (FailTag.IsValid()) Tags.AddTag(FailTag);
				UAbilitySystemBlueprintLibrary::RemoveLooseGameplayTags(Actor, Tags);
			}
		}), Cooldown, false);
	}
}

// ------------------------------------------------------------
// --------------------- MINIGAME -----------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::StartMinigame(AActor* Instigator, AActor* Target, float Duration)
{
	if (Target != GetOwner() || !CheckTamePrerequisites(Instigator) || IsValid(ActiveMinigameWidget)) return;
	CachedInstigator = Instigator;
	APawn* Pawn = Cast<APawn>(Instigator);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] No valid PlayerController for CreateWidget."));
		ClearTameTags();
		return;
	}

	UANSUIPlayerSubsystem* UI = PC->GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>();
	UCommonActivatableWidget* Page = UI ? UI->SpawnWidgetByTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Pangea.Taming"))) : nullptr;
	if (UTamingWidget* Widget = Cast<UTamingWidget>(Page))
	{
		Widget->Duration = FMath::Max(0.1f, Duration);
		Widget->OnResult.AddDynamic(this, &UPangeaTamingComponent::OnMinigameResult);
		ActiveMinigameWidget = Widget;
		Widget->StartCountdown(Duration);
		UE_LOG(LogTemp, Log, TEXT("[TamingComponent] Minigame started for %s"), *GetNameSafe(Target));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingComponent] Failed to create TamingWidget instance."));
		ClearTameTags();
	}
}

void UPangeaTamingComponent::OnMinigameResult(bool bSuccess)
{
    if (!ActiveMinigameWidget) return;
    // Release our reference before deactivation so cancellation cannot re-enter.
    UTamingWidget* Widget = Cast<UTamingWidget>(ActiveMinigameWidget);
    ActiveMinigameWidget = nullptr;
    if (Widget)
    {
        Widget->OnResult.RemoveDynamic(this, &UPangeaTamingComponent::OnMinigameResult);
        if (UANSUIPlayerSubsystem* UI = GetWorld()->GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>()) UI->RemoveInGameWidget(Widget, false, false);
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

	if (NewRole != ETamedRole::None)
		NewRole = DetermineFinalRole(NewRole);
	TamedRole = NewRole;
	ApplyRoleTag(NewRole);
	OnTameRoleSelected.Broadcast(TamedRole);

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		switch (NewRole)
		{
		case ETamedRole::Mount:
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

bool UPangeaTamingComponent::CanBeTamed() const
{
	return TameSpeciesConfig && 
		   (TameSpeciesConfig->bCanBeMount || TameSpeciesConfig->bCanBeCompanion);
}

ETamedRole UPangeaTamingComponent::DetermineFinalRole(ETamedRole DesiredRole) const
{
	if (!TameSpeciesConfig)
		return ETamedRole::None;
	if ((DesiredRole == ETamedRole::Mount && TameSpeciesConfig->bCanBeMount) ||
		(DesiredRole == ETamedRole::Companion && TameSpeciesConfig->bCanBeCompanion))
		return DesiredRole;

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
	switch (TamedState)
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
	if (!TameSpeciesConfig || !CachedInstigator.IsValid() || !TameSpeciesConfig->RequiredTamingItem || TameSpeciesConfig->RequiredItemCount <= 0) return;

	if (UACFInventoryComponent* Inv = CachedInstigator->FindComponentByClass<UACFInventoryComponent>())
	{
		// ACF inventory entries are GUID-identified. A class-only temporary item cannot be removed.
		Inv->ConsumeItems({FBaseItem(TameSpeciesConfig->RequiredTamingItem, TameSpeciesConfig->RequiredItemCount)});
	}
}

// ------------------------------------------------------------
// --------------------- UTILITIES ----------------------------
// ------------------------------------------------------------

void UPangeaTamingComponent::ChangeTeam(const FGameplayTag& TeamTag) 
{
	if (UACFTeamComponent* TeamComp = GetOwner()->FindComponentByClass<UACFTeamComponent>())
	{
		if (TeamTag.IsValid())
		{
			TeamComp->SetTeam(TeamTag);
			
			// Persist the selected team for SaveGame
			//ChangedTeam = TeamTag;
		}
	}
}

void UPangeaTamingComponent::GrantTamedAbilities() const
{
	if (!TameSpeciesConfig) return;
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		for (const TSubclassOf<UGameplayAbility>& Ability : TameSpeciesConfig->AbilitiesGrantedWhenTamed)
			if (Ability && GetOwner()->HasAuthority() && !ASC->FindAbilitySpecFromClass(Ability))
				ASC->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE));
	}
}

void UPangeaTamingComponent::SwitchAIController(TSubclassOf<AAIController> NewControllerClass)
{

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->HasAuthority() || !NewControllerClass) return;
	// Never destroy a player's controller (including when a tamed mount is possessed).
	if (Pawn->GetController() && (!Pawn->GetController()->IsA<AAIController>() || Pawn->GetController()->IsA(NewControllerClass))) return;

	if (AController* Old = Pawn->GetController())
	{
		Old->UnPossess();
		Old->Destroy();
	}

	if (AAIController* NewAIC = Pawn->GetWorld()->SpawnActor<AAIController>(NewControllerClass, Pawn->GetActorLocation(), Pawn->GetActorRotation()))
	{
		NewAIC->Possess(Pawn);
		//CurrentAIControllerClass = NewControllerClass;
	}
	
}
