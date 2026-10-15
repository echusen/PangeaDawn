// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFCheatManager.h"
#include "AMSMapSubsystem.h"
#include "AQSQuestManagerComponent.h"
#include "AQSQuestObjective.h"
#include "ARSLevelingComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ACFCurrencyComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Components/ACFInventoryComponent.h"
#include "Items/ACFItem.h"
#include "UObject/UObjectIterator.h"
#include "Logging.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "TimerManager.h"
#include <Engine/GameInstance.h>
#include <Kismet/GameplayStatics.h>
#include <GameFramework/Pawn.h>
#include <NavigationSystem.h>


void UACFCheatManager::DiscoverAllLocations()
{
    UAMSMapSubsystem* mapSubsystem = UGameplayStatics::GetGameInstance(GetOuterAPlayerController())->GetSubsystem<UAMSMapSubsystem>();
    mapSubsystem->DiscoverAllLocation();
}



void UACFCheatManager::AddExp(float Exps)
{
    UARSLevelingComponent* levelComp = GetOuterAPlayerController()->GetPawn()->FindComponentByClass<UARSLevelingComponent>();

    if (levelComp) {
        levelComp->AddExp(Exps);
    }
}

void UACFCheatManager::AddCurrency(float Coins)
{
    UACFCurrencyComponent* coinComp = GetOuterAPlayerController()->GetPawn()->FindComponentByClass<UACFCurrencyComponent>();

    if (coinComp) {
        coinComp->AddCurrency(Coins);
    }
}

void UACFCheatManager::ProceedInTrackedQuest()
{
    UAQSQuestManagerComponent* questComp = GetOuterAPlayerController()->FindComponentByClass<UAQSQuestManagerComponent>();
    if (questComp) {
        questComp->DEBUG_ProceedTrackedQuest();
    }
}

void UACFCheatManager::TeleportToTarget(float Distance)
{
    APlayerController* PC = GetOuterAPlayerController();
    if (!PC) {
        return;
    }

    APawn* Pawn = PC->GetPawn();
    if (!Pawn) {
        UE_LOG(LogAscentCheatManager, Warning, TEXT("TeleportToTarget: no possessed pawn."));
        return;
    }

    UAQSQuestManagerComponent* QuestComp = PC->FindComponentByClass<UAQSQuestManagerComponent>();
    if (!QuestComp) {
        UE_LOG(LogAscentCheatManager, Warning, TEXT("TeleportToTarget: PlayerController has no UAQSQuestManagerComponent."));
        return;
    }

    if (!QuestComp->GetCurrentlyTrackedQuest()) {
        UE_LOG(LogAscentCheatManager, Warning, TEXT("TeleportToTarget: no quest is currently tracked."));
        return;
    }

    const float ArrivalDistance = FMath::Max(Distance, 50.f);
    const FVector PawnLoc = Pawn->GetActorLocation();

    AActor* ClosestActor = nullptr;
    FVector TargetLocation = FVector::ZeroVector;
    float BestDistSq = TNumericLimits<float>::Max();
    bool bFoundTarget = false;

    const TArray<AActor*> LiveTargets = QuestComp->GetCurrentlyTrackedQuestObjectivesTargetActors();
    for (AActor* TargetActor : LiveTargets) {
        if (!IsValid(TargetActor)) {
            continue;
        }

        const float DistSq = FVector::DistSquared(PawnLoc, TargetActor->GetActorLocation());
        if (DistSq < BestDistSq) {
            BestDistSq = DistSq;
            ClosestActor = TargetActor;
            TargetLocation = TargetActor->GetActorLocation();
            bFoundTarget = true;
        }
    }

    if (!bFoundTarget) {
        for (UAQSQuestObjective* Objective : QuestComp->GetCurrentlyTrackedQuestObjectives()) {
            if (!Objective) {
                continue;
            }

            for (const FVector& CachedLocation : Objective->GetKnownTargetLocations()) {
                const float DistSq = FVector::DistSquared(PawnLoc, CachedLocation);
                if (DistSq < BestDistSq) {
                    BestDistSq = DistSq;
                    TargetLocation = CachedLocation;
                    bFoundTarget = true;
                }
            }
        }
    }

    if (!bFoundTarget) {
        UE_LOG(LogAscentCheatManager, Warning, TEXT("TeleportToTarget: tracked quest has no known objective target location."));
        return;
    }

    FVector OffsetDir = FVector::ForwardVector;
    if (ClosestActor) {
        const FVector ActorForward = ClosestActor->GetActorForwardVector();
        OffsetDir = ActorForward.GetSafeNormal2D();
        if (OffsetDir.IsNearlyZero()) {
            OffsetDir = FVector::ForwardVector;
        }

        FVector Origin;
        FVector Extent;
        ClosestActor->GetActorBounds(true, Origin, Extent);
        TargetLocation = Origin;
        const float HorizontalRadius = FMath::Max(Extent.X, Extent.Y);
        OffsetDir *= (HorizontalRadius + ArrivalDistance);
    }
    else {
        OffsetDir *= ArrivalDistance;
    }

    FVector DestLocation = TargetLocation + OffsetDir;
    FRotator DestRotation = (TargetLocation - DestLocation).GetSafeNormal2D().Rotation();
    DestRotation.Pitch = 0.f;
    DestRotation.Roll = 0.f;

    PendingNavSnapRadius = FMath::Max(ArrivalDistance * 4.f, 1000.f);
    SnapLocationToNavMesh(PC, Pawn, DestLocation, PendingNavSnapRadius);

    constexpr float HoverHeight = 200.f;
    FVector HoverLocation = DestLocation;
    HoverLocation.Z += HoverHeight;

    TeleportPawnIgnoringPhysics(Pawn, HoverLocation, DestRotation);
    DisablePawnGravityTemporarily(Pawn);

    UE_LOG(LogAscentCheatManager, Log, TEXT("TeleportToTarget: teleported to %s (hovering %.1fs for terrain load)"),
        *HoverLocation.ToCompactString(), TeleportTerrainLoadDuration);
}

void UACFCheatManager::TeleportPawnIgnoringPhysics(APawn* Pawn, const FVector& Dest, const FRotator& Rot)
{
    if (!Pawn) {
        return;
    }

    if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) {
        Movement->StopMovementImmediately();
    }

    Pawn->SetActorLocationAndRotation(Dest, Rot, false, nullptr, ETeleportType::TeleportPhysics);
}

bool UACFCheatManager::SnapLocationToNavMesh(UObject* WorldContext, APawn* Pawn, FVector& InOutLocation, float SearchRadius)
{
    if (!WorldContext) {
        return false;
    }

    const float Radius = FMath::Max(SearchRadius, 100.f);
    const FVector ProjectExtent(FMath::Max(Radius, 500.f), FMath::Max(Radius, 500.f), 2500.f);

    FVector NavLocation;
    bool bFound = UNavigationSystemV1::K2_ProjectPointToNavigation(WorldContext, InOutLocation, NavLocation, nullptr, nullptr, ProjectExtent);
    if (!bFound) {
        bFound = UNavigationSystemV1::K2_GetRandomReachablePointInRadius(WorldContext, InOutLocation, NavLocation, Radius);
    }

    if (!bFound) {
        UWorld* World = Pawn ? Pawn->GetWorld() : (WorldContext ? WorldContext->GetWorld() : nullptr);
        if (World) {
            FHitResult FloorHit;
            const FVector TraceStart = InOutLocation + FVector(0.f, 0.f, 500.f);
            const FVector TraceEnd = InOutLocation - FVector(0.f, 0.f, 5000.f);
            FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TeleportToTargetNavSnap), false, Pawn);
            if (World->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams)) {
                NavLocation = FloorHit.ImpactPoint;
                bFound = true;
            }
        }
    }

    if (!bFound) {
        return false;
    }

    if (ACharacter* Character = Cast<ACharacter>(Pawn)) {
        if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent()) {
            NavLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
        }
    }

    InOutLocation = NavLocation;
    return true;
}

void UACFCheatManager::DisablePawnGravityTemporarily(APawn* Pawn)
{
    ACharacter* Character = Cast<ACharacter>(Pawn);
    if (!Character) {
        return;
    }

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!Movement) {
        return;
    }

    UWorld* World = GetOuterAPlayerController() ? GetOuterAPlayerController()->GetWorld() : nullptr;
    if (!World) {
        return;
    }

    World->GetTimerManager().ClearTimer(GravityRestoreTimerHandle);

    if (!GravityDisabledMovement.IsValid()) {
        CachedGravityScale = Movement->GravityScale;
        CachedMovementMode = Movement->MovementMode;
    }

    GravityDisabledMovement = Movement;
    GravityDisabledPawn = Pawn;

    Movement->SetMovementMode(MOVE_Flying);
    Movement->GravityScale = 0.f;
    Movement->StopMovementImmediately();

    const float LoadDuration = FMath::Max(TeleportTerrainLoadDuration, 0.f);
    if (LoadDuration <= 0.f) {
        FinishDelayedTeleport();
        return;
    }

    World->GetTimerManager().SetTimer(
        GravityRestoreTimerHandle,
        this,
        &UACFCheatManager::FinishDelayedTeleport,
        LoadDuration,
        false);
}

void UACFCheatManager::RestorePawnMovement()
{
    if (UCharacterMovementComponent* Movement = GravityDisabledMovement.Get()) {
        Movement->GravityScale = CachedGravityScale;
        Movement->SetMovementMode(CachedMovementMode);
    }

    GravityDisabledMovement.Reset();
    GravityDisabledPawn.Reset();
    CachedGravityScale = 1.f;
    CachedMovementMode = MOVE_Walking;

    if (APlayerController* PC = GetOuterAPlayerController()) {
        if (UWorld* World = PC->GetWorld()) {
            World->GetTimerManager().ClearTimer(GravityRestoreTimerHandle);
        }
    }
}

void UACFCheatManager::FinishDelayedTeleport()
{
    APawn* Pawn = GravityDisabledPawn.Get();
    bool bSnapped = false;
    if (Pawn) {
        FVector SnapLocation = Pawn->GetActorLocation();
        if (SnapLocationToNavMesh(Pawn, Pawn, SnapLocation, PendingNavSnapRadius)) {
            TeleportPawnIgnoringPhysics(Pawn, SnapLocation, Pawn->GetActorRotation());
            bSnapped = true;
        }
    }

    if (bSnapped) {
        CachedMovementMode = MOVE_Walking;
    }

    RestorePawnMovement();
}

void UACFCheatManager::StartQuests()
{
    UAQSQuestManagerComponent* questComp = GetOuterAPlayerController()->FindComponentByClass<UAQSQuestManagerComponent>();
    if (questComp) {
        for (const FGameplayTag& questTag : QuestsToStart) {
            questComp->StartQuest(questTag);
        }
    }
}

void UACFCheatManager::AddItemsToInventory()
{
    UACFEquipmentComponent* equipComp = GetOuterAPlayerController()->GetPawn()->FindComponentByClass<UACFEquipmentComponent>();

    if (equipComp) {
        for (const FBaseItem& item : ItemsToAdd) {
            equipComp->AddItemToInventory(item);
        }
    }
}

void UACFCheatManager::CheatMessage(const FString& Message, bool bWarning)
{
    if (bWarning) {
        UE_LOG(LogAscentCheatManager, Warning, TEXT("%s"), *Message);
    }
    else {
        UE_LOG(LogAscentCheatManager, Log, TEXT("%s"), *Message);
    }

    if (APlayerController* PC = GetOuterAPlayerController()) {
        PC->ClientMessage(Message);
    }
}

int32 UACFCheatManager::NormalizeGiveQuantity(int32 Quantity)
{
    if (Quantity <= 0) {
        CheatMessage(TEXT("GiveItem: quantity must be at least 1."), true);
        return 0;
    }

    if (Quantity > 9999) {
        CheatMessage(TEXT("GiveItem: quantity clamped to 9999."), true);
        return 9999;
    }

    return Quantity;
}

bool UACFCheatManager::IsGrantableItemClass(UClass* ItemClass) const
{
    if (!ItemClass || !ItemClass->IsChildOf(UACFItem::StaticClass()) || ItemClass == UACFItem::StaticClass()) {
        return false;
    }

    if (ItemClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) {
        return false;
    }

    const FString ClassName = ItemClass->GetName();
    return !ClassName.StartsWith(TEXT("SKEL_")) && !ClassName.StartsWith(TEXT("REINST_"));
}

UClass* UACFCheatManager::FindItemClassForGive(const FString& Query, bool bMatchBlueprintName)
{
    const FString TrimmedQuery = Query.TrimStartAndEnd();
    if (TrimmedQuery.IsEmpty()) {
        CheatMessage(TEXT("GiveItem: item name is empty."), true);
        return nullptr;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& Registry = AssetRegistryModule.Get();

    TArray<FTopLevelAssetPath> BaseClasses;
    BaseClasses.Add(UACFItem::StaticClass()->GetClassPathName());

    TSet<FTopLevelAssetPath> DerivedClasses;
    Registry.GetDerivedClassNames(BaseClasses, TSet<FTopLevelAssetPath>(), DerivedClasses);

    TArray<FTopLevelAssetPath> ClassPaths;
    TMap<FString, UClass*> LoadedClasses;
    TSet<FString> SeenPaths;

    for (const FTopLevelAssetPath& ClassPath : DerivedClasses) {
        if (!ClassPath.IsValid()) {
            continue;
        }

        const FString PathKey = ClassPath.ToString();
        if (SeenPaths.Contains(PathKey)) {
            continue;
        }

        SeenPaths.Add(PathKey);
        ClassPaths.Add(ClassPath);
    }

    for (TObjectIterator<UClass> It; It; ++It) {
        UClass* Class = *It;
        if (!IsGrantableItemClass(Class) || Class->GetOutermost() == GetTransientPackage()) {
            continue;
        }

        const FTopLevelAssetPath ClassPath = Class->GetClassPathName();
        const FString PathKey = ClassPath.ToString();
        if (!SeenPaths.Contains(PathKey)) {
            SeenPaths.Add(PathKey);
            ClassPaths.Add(ClassPath);
        }

        LoadedClasses.Add(PathKey, Class);
    }

    if (ClassPaths.Num() == 0) {
        CheatMessage(TEXT("GiveItem: no item classes are registered yet. Wait for the asset scan to finish, then try again."), true);
        return nullptr;
    }

    if (bMatchBlueprintName) {
        TArray<FTopLevelAssetPath> Matches;
        TArray<FString> Suggestions;
        for (const FTopLevelAssetPath& ClassPath : ClassPaths) {
            FString BlueprintId = ClassPath.GetAssetName().ToString();
            if (BlueprintId.EndsWith(TEXT("_C"))) {
                BlueprintId.LeftChopInline(2);
            }

            const FString ClassName = ClassPath.GetAssetName().ToString();
            const FString PackagePath = ClassPath.GetPackageName().ToString();
            const FString FullPath = ClassPath.ToString();
            const bool bExact = BlueprintId.Equals(TrimmedQuery, ESearchCase::IgnoreCase)
                || ClassName.Equals(TrimmedQuery, ESearchCase::IgnoreCase)
                || PackagePath.Equals(TrimmedQuery, ESearchCase::IgnoreCase)
                || FullPath.Equals(TrimmedQuery, ESearchCase::IgnoreCase);

            if (bExact) {
                Matches.Add(ClassPath);
            }
            else if (TrimmedQuery.Len() >= 3 && BlueprintId.Contains(TrimmedQuery, ESearchCase::IgnoreCase) && Suggestions.Num() < 32) {
                Suggestions.Add(FString::Printf(TEXT("%s (%s)"), *BlueprintId, *FullPath));
            }
        }

        if (Matches.Num() > 1) {
            CheatMessage(FString::Printf(TEXT("GiveItem: '%s' matches more than one blueprint. Pass the full class path."), *TrimmedQuery), true);
            for (const FTopLevelAssetPath& Match : Matches) {
                CheatMessage(FString::Printf(TEXT("  %s"), *Match.ToString()), true);
            }
            return nullptr;
        }

        if (Matches.Num() == 0) {
            CheatMessage(FString::Printf(TEXT("GiveItem: no blueprint named '%s'."), *TrimmedQuery), true);
            Suggestions.Sort();
            const int32 Shown = FMath::Min(Suggestions.Num(), 8);
            if (Shown > 0) {
                CheatMessage(TEXT("Closest blueprints:"), true);
                for (int32 SuggestionIndex = 0; SuggestionIndex < Shown; ++SuggestionIndex) {
                    CheatMessage(FString::Printf(TEXT("  %s"), *Suggestions[SuggestionIndex]), true);
                }
                if (Suggestions.Num() > Shown) {
                    CheatMessage(TEXT("  ..."), true);
                }
            }
            return nullptr;
        }

        const FString PathKey = Matches[0].ToString();
        UClass* Loaded = LoadedClasses.FindRef(PathKey);
        if (!Loaded) {
            Loaded = LoadObject<UClass>(nullptr, *PathKey);
        }
        if (!Loaded) {
            Loaded = StaticLoadClass(UACFItem::StaticClass(), nullptr, *PathKey);
        }
        if (!IsGrantableItemClass(Loaded)) {
            CheatMessage(FString::Printf(TEXT("GiveItem: '%s' is not a concrete item blueprint."), *TrimmedQuery), true);
            return nullptr;
        }
        return Loaded;
    }

    TArray<UClass*> NameMatches;
    TArray<FString> NameSuggestions;
    for (const FTopLevelAssetPath& ClassPath : ClassPaths) {
        const FString PathKey = ClassPath.ToString();
        UClass* Loaded = LoadedClasses.FindRef(PathKey);
        if (!Loaded) {
            Loaded = LoadObject<UClass>(nullptr, *PathKey);
        }
        if (!Loaded) {
            Loaded = StaticLoadClass(UACFItem::StaticClass(), nullptr, *PathKey);
        }
        if (!IsGrantableItemClass(Loaded)) {
            continue;
        }

        const UACFItem* DefaultItem = GetDefault<UACFItem>(Loaded);
        const FString DisplayName = DefaultItem ? DefaultItem->GetItemName().ToString() : FString();
        if (DisplayName.IsEmpty()) {
            continue;
        }

        FString BlueprintId = Loaded->GetName();
        if (BlueprintId.EndsWith(TEXT("_C"))) {
            BlueprintId.LeftChopInline(2);
        }

        if (DisplayName.Equals(TrimmedQuery, ESearchCase::IgnoreCase)) {
            NameMatches.Add(Loaded);
        }
        else if (TrimmedQuery.Len() >= 3 && DisplayName.Contains(TrimmedQuery, ESearchCase::IgnoreCase) && NameSuggestions.Num() < 32) {
            NameSuggestions.Add(FString::Printf(TEXT("%s [%s]"), *DisplayName, *BlueprintId));
        }
    }

    if (NameMatches.Num() > 1) {
        CheatMessage(FString::Printf(TEXT("GiveItem: '%s' matches more than one item. Use GiveItem -BP and the blueprint name."), *TrimmedQuery), true);
        for (UClass* Match : NameMatches) {
            const UACFItem* DefaultItem = GetDefault<UACFItem>(Match);
            const FString DisplayName = DefaultItem ? DefaultItem->GetItemName().ToString() : Match->GetName();
            FString BlueprintId = Match->GetName();
            if (BlueprintId.EndsWith(TEXT("_C"))) {
                BlueprintId.LeftChopInline(2);
            }
            CheatMessage(FString::Printf(TEXT("  %s [%s]"), *DisplayName, *BlueprintId), true);
        }
        return nullptr;
    }

    if (NameMatches.Num() == 1) {
        return NameMatches[0];
    }

    CheatMessage(FString::Printf(TEXT("GiveItem: no item named '%s'."), *TrimmedQuery), true);
    NameSuggestions.Sort();
    const int32 Shown = FMath::Min(NameSuggestions.Num(), 8);
    if (Shown > 0) {
        CheatMessage(TEXT("Closest names:"), true);
        for (int32 SuggestionIndex = 0; SuggestionIndex < Shown; ++SuggestionIndex) {
            CheatMessage(FString::Printf(TEXT("  %s"), *NameSuggestions[SuggestionIndex]), true);
        }
        if (NameSuggestions.Num() > Shown) {
            CheatMessage(TEXT("  ..."), true);
        }
    }
    return nullptr;
}

void UACFCheatManager::GrantItemToPawn(UClass* ItemClass, int32 Quantity)
{
    if (!ItemClass) {
        return;
    }

    APlayerController* PC = GetOuterAPlayerController();
    if (!PC) {
        UE_LOG(LogAscentCheatManager, Warning, TEXT("GiveItem: no player controller."));
        return;
    }

    APawn* Pawn = PC->GetPawn();
    if (!Pawn) {
        CheatMessage(TEXT("GiveItem: no possessed pawn."), true);
        return;
    }

    UACFInventoryComponent* Inventory = Pawn->FindComponentByClass<UACFInventoryComponent>();
    if (!Inventory) {
        CheatMessage(TEXT("GiveItem: pawn has no inventory component."), true);
        return;
    }

    Inventory->AddItemToInventoryByClass(ItemClass, Quantity, true);

    const UACFItem* DefaultItem = GetDefault<UACFItem>(ItemClass);
    const FString DisplayName = DefaultItem ? DefaultItem->GetItemName().ToString() : FString();
    FString BlueprintId = ItemClass->GetName();
    if (BlueprintId.EndsWith(TEXT("_C"))) {
        BlueprintId.LeftChopInline(2);
    }

    const FString Label = DisplayName.IsEmpty() ? BlueprintId : FString::Printf(TEXT("%s [%s]"), *DisplayName, *BlueprintId);
    CheatMessage(FString::Printf(TEXT("GiveItem: added %d x %s."), Quantity, *Label), false);
}

void UACFCheatManager::GiveItemByName(const FString& ItemName, int32 Quantity)
{
    Quantity = NormalizeGiveQuantity(Quantity);
    if (Quantity <= 0) {
        return;
    }

    UClass* ItemClass = FindItemClassForGive(ItemName, false);
    if (ItemClass) {
        GrantItemToPawn(ItemClass, Quantity);
    }
}

void UACFCheatManager::GiveItemByBlueprint(const FString& BlueprintName, int32 Quantity)
{
    Quantity = NormalizeGiveQuantity(Quantity);
    if (Quantity <= 0) {
        return;
    }

    UClass* ItemClass = FindItemClassForGive(BlueprintName, true);
    if (ItemClass) {
        GrantItemToPawn(ItemClass, Quantity);
    }
}

void UACFCheatManager::Give(const FString& Args)
{
    FString Rest = Args.TrimStartAndEnd();
    if (Rest.StartsWith(TEXT("item"), ESearchCase::IgnoreCase)) {
        const TCHAR Next = Rest.Len() > 4 ? Rest[4] : TEXT('\0');
        if (Next == TEXT('\0') || FChar::IsWhitespace(Next)) {
            Rest = Rest.Mid(4).TrimStart();
        }
    }

    GiveItem(Rest);
}

void UACFCheatManager::GiveItem(const FString& Args)
{
    const FString Usage = TEXT("Usage: GiveItem <Display Name> [Quantity] | GiveItem -BP <Blueprint Name> [Quantity]");

    TArray<FString> Tokens;
    const TCHAR* Cursor = *Args;
    FString Token;
    while (FParse::Token(Cursor, Token, true)) {
        Tokens.Add(Token);
    }

    if (Tokens.Num() == 0) {
        CheatMessage(Usage, true);
        return;
    }

    bool bBlueprint = false;
    bool bQuantitySet = false;
    int32 Quantity = 1;
    TArray<FString> NameParts;

    for (int32 Index = 0; Index < Tokens.Num(); ++Index) {
        FString Key = Tokens[Index];
        FString InlineValue;
        bool bInline = false;
        int32 EqualsIndex = INDEX_NONE;
        if (Key.FindChar(TEXT('='), EqualsIndex) && EqualsIndex > 0) {
            InlineValue = Key.Mid(EqualsIndex + 1).TrimStartAndEnd();
            if (InlineValue.Len() >= 2 && InlineValue.StartsWith(TEXT("\"")) && InlineValue.EndsWith(TEXT("\""))) {
                InlineValue = InlineValue.Mid(1, InlineValue.Len() - 2);
            }
            Key = Key.Left(EqualsIndex);
            bInline = true;
            if (InlineValue.IsEmpty()) {
                CheatMessage(FString::Printf(TEXT("GiveItem: missing value for %s. %s"), *Key, *Usage), true);
                return;
            }
        }

        const bool bBlueprintFlag = Key.Equals(TEXT("-BP"), ESearchCase::IgnoreCase) || Key.Equals(TEXT("--BP"), ESearchCase::IgnoreCase);
        const bool bQuantityFlag = Key.Equals(TEXT("--quantity"), ESearchCase::IgnoreCase)
            || Key.Equals(TEXT("-quantity"), ESearchCase::IgnoreCase)
            || Key.Equals(TEXT("--count"), ESearchCase::IgnoreCase)
            || Key.Equals(TEXT("-count"), ESearchCase::IgnoreCase)
            || Key.Equals(TEXT("-q"), ESearchCase::IgnoreCase);

        if (bBlueprintFlag) {
            bBlueprint = true;
            if (bInline) {
                NameParts.Add(InlineValue);
            }
            continue;
        }

        if (bQuantityFlag) {
            if (bQuantitySet) {
                CheatMessage(TEXT("GiveItem: quantity was passed more than once."), true);
                return;
            }

            FString QuantityText = InlineValue;
            if (!bInline) {
                if (Index + 1 >= Tokens.Num()) {
                    CheatMessage(FString::Printf(TEXT("GiveItem: missing quantity. %s"), *Usage), true);
                    return;
                }
                ++Index;
                QuantityText = Tokens[Index];
            }

            bool bDigits = !QuantityText.IsEmpty();
            for (const TCHAR Char : QuantityText) {
                if (!FChar::IsDigit(Char)) {
                    bDigits = false;
                    break;
                }
            }
            if (!bDigits) {
                CheatMessage(FString::Printf(TEXT("GiveItem: quantity '%s' is not a positive whole number. %s"), *QuantityText, *Usage), true);
                return;
            }

            Quantity = QuantityText.Len() > 6 ? 9999 : FCString::Atoi(*QuantityText);
            bQuantitySet = true;
            continue;
        }

        if (Key.StartsWith(TEXT("-"))) {
            CheatMessage(FString::Printf(TEXT("GiveItem: unknown option '%s'. %s"), *Key, *Usage), true);
            return;
        }

        NameParts.Add(bInline ? InlineValue : Tokens[Index]);
    }

    if (!bQuantitySet && NameParts.Num() >= 2) {
        const FString& Last = NameParts.Last();
        bool bDigits = !Last.IsEmpty();
        for (const TCHAR Char : Last) {
            if (!FChar::IsDigit(Char)) {
                bDigits = false;
                break;
            }
        }
        if (bDigits) {
            Quantity = Last.Len() > 6 ? 9999 : FCString::Atoi(*Last);
            NameParts.Pop();
        }
    }

    const FString Query = FString::Join(NameParts, TEXT(" "));
    if (Query.IsEmpty()) {
        CheatMessage(Usage, true);
        return;
    }

    if (bBlueprint) {
        GiveItemByBlueprint(Query, Quantity);
    }
    else {
        GiveItemByName(Query, Quantity);
    }
}
