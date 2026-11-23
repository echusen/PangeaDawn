// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFItemsManagerComponent.h"
#include "ACFBuildableComponent.h"
#include "ACFCraftingComponent.h"
#include "ACFItemSystemFunctionLibrary.h"
#include "ACFVendorComponent.h"
#include "Components/ACFCurrencyComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Components/ACFInventoryComponent.h"
#include "Engine/DataTable.h"
#include "GameplayTagsManager.h"
#include "Items/ACFItem.h"
#include "Kismet/KismetMathLibrary.h"
#include "Preview/ACFCharacterPreviewStage.h"
#include <GameFramework/Pawn.h>
#include <Engine/AssetManager.h>
#include <GameFramework/Character.h>
#include "ACFMountSystemFunctionLibrary.h"
#include "Data/ACFCharacterInitializerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace {
	const FName AppearanceCopyTag(TEXT("ACFAppearanceCopy"));
}

// Sets default values for this component's properties
UACFItemsManagerComponent::UACFItemsManagerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

// Called when the game starts
void UACFItemsManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	SpawnItemPreviewStage();
}

void UACFItemsManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PreviewStreamableHandle.IsValid()) {
		PreviewStreamableHandle->CancelHandle();
		PreviewStreamableHandle.Reset();
	}
	if (AACFCharacter* Player = UACFMountSystemFunctionLibrary::GetLocalRiderPlayerCharacter(this)) {
		if (UACFEquipmentComponent* Equipment = Player->FindComponentByClass<UACFEquipmentComponent>()) {
			Equipment->OnEquipmentChanged.RemoveDynamic(this, &UACFItemsManagerComponent::ApplyAppearance);
		}
	}
	if (IsValid(ItemPreviewLight)) {
		ItemPreviewLight->DestroyComponent();
	}
	ItemPreviewLight = nullptr;
	if (IsValid(PreviewCharacter)) {
		PreviewCharacter->Destroy();
	}
	PreviewCharacter = nullptr;
	if (IsValid(ItemPreviewStage)) {
		ItemPreviewStage->Destroy();
	}
	ItemPreviewStage = nullptr;
	Super::EndPlay(EndPlayReason);
}

USpotLightComponent* UACFItemsManagerComponent::SpawnItemPreviewLight()
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer
		|| !IsValid(ItemPreviewStage) || !ItemPreviewStage->GetRootComponent()
		|| !IsValid(PreviewCharacter) || !PreviewCharacter->GetRootComponent()) {
		return nullptr;
	}
	if (!IsValid(ItemPreviewLight)) {
		if (!bEnableItemPreviewLight) {
			return nullptr;
		}
		ItemPreviewLight = NewObject<USpotLightComponent>(ItemPreviewStage, NAME_None, RF_Transient);
		if (!ItemPreviewLight) {
			return nullptr;
		}
		ItemPreviewLight->SetMobility(EComponentMobility::Movable);
		ItemPreviewLight->SetupAttachment(ItemPreviewStage->GetRootComponent());
		ItemPreviewStage->AddInstanceComponent(ItemPreviewLight);
		ItemPreviewLight->RegisterComponent();
	}
	UpdateItemPreviewLightSettings();
	return ItemPreviewLight;
}

void UACFItemsManagerComponent::UpdateItemPreviewLightSettings()
{
	if (!IsValid(ItemPreviewLight) || !IsValid(PreviewCharacter)) {
		return;
	}
	const FTransform CharacterTransform = PreviewCharacter->GetActorTransform();
	const FVector LightLocation = CharacterTransform.TransformPosition(ItemPreviewLightOffset);
	const FVector TargetLocation = CharacterTransform.TransformPosition(ItemPreviewLightTargetOffset);
	const FVector AimDirection = TargetLocation - LightLocation;
	ItemPreviewLight->SetWorldLocationAndRotation(LightLocation,
		AimDirection.IsNearlyZero() ? FRotator(-90.f, 0.f, 0.f) : AimDirection.Rotation());
	ItemPreviewLight->SetIntensityUnits(ELightUnits::Lumens);
	ItemPreviewLight->SetIntensity(FMath::Max(0.f, ItemPreviewLightIntensity));
	ItemPreviewLight->SetLightColor(ItemPreviewLightColor);
	ItemPreviewLight->SetAttenuationRadius(FMath::Max(1.f, ItemPreviewLightAttenuationRadius));
	const float OuterAngle = FMath::Clamp(ItemPreviewLightOuterConeAngle, 1.f, 89.f);
	ItemPreviewLight->SetOuterConeAngle(OuterAngle);
	ItemPreviewLight->SetInnerConeAngle(FMath::Clamp(ItemPreviewLightInnerConeAngle, 0.f, OuterAngle));
	ItemPreviewLight->SetCastShadows(false);
	ItemPreviewLight->SetVisibility(bEnableItemPreviewLight);
	if (USceneCaptureComponent2D* Capture = PreviewCharacter->FindComponentByClass<USceneCaptureComponent2D>()) {
		// Explicit refresh also works while inventory UI has paused gameplay.
		if (Capture->TextureTarget) {
			Capture->CaptureScene();
		}
	}
}

#if WITH_EDITOR
void UACFItemsManagerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetMemberPropertyName();
	if (GetWorld() && GetWorld()->IsGameWorld()
		&& (PropertyName.ToString().StartsWith(TEXT("ItemPreviewLight"))
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UACFItemsManagerComponent, bEnableItemPreviewLight))) {
		// Final Details-panel edits normally rerun the controller's construction script.
		// That destroys/recreates this component, triggering EndPlay and deleting the
		// preview actors. Keep the normal re-registration, but suppress reconstruction
		// for live preview lighting edits, as UE already does for interactive edits.
		FPropertyChangedEvent RuntimeEvent = PropertyChangedEvent;
		RuntimeEvent.ChangeType = EPropertyChangeType::Interactive;
		Super::PostEditChangeProperty(RuntimeEvent);
		SpawnItemPreviewLight();
		return;
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UACFItemsManagerComponent::SetItemPreviewLightEnabled(bool bEnabled)
{
	bEnableItemPreviewLight = bEnabled;
	SpawnItemPreviewLight();
}

bool UACFItemsManagerComponent::GenerateItemsFromRules(const TArray<FACFItemGenerationRule>& generationRules, TArray<FBaseItem>& outItems)
{
	outItems.Empty();
	bool bIsSuccessful = true;

	if (generationRules.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("Missing generation rules! - UACFItemsManagerComponent"));
		return false;
	}

	for (auto genRule : generationRules) {
		FBaseItem outItem;
		if (GenerateItemFromRule(genRule, outItem)) {
			outItems.Add(outItem);
		}
		else {
			bIsSuccessful = false;
		}
	}
	return bIsSuccessful;
}

bool UACFItemsManagerComponent::GenerateItemFromRule(const FACFItemGenerationRule& generationRules, FBaseItem& outItem)
{
	TArray<FItemGenerationSlot> matchingItems;
	if (GetItemsDB()) {
		const auto items = GetItemsDB()->GetRowMap();
		for (const auto& itemItr : items) {
			FItemGenerationSlot* itemSlot = (FItemGenerationSlot*)itemItr.Value;
			if (itemSlot) {
				if (DoesSlotMatchesRule(generationRules, *itemSlot)) {
					matchingItems.Add(*itemSlot);
				}
			}
			else {
				return false;
			}
		}

		if (matchingItems.Num() == 0) {
			UE_LOG(LogTemp, Warning, TEXT("No Matching Items in DB! - UACFItemsManagerComponent"));
			return false;
		}

		const int32 selectedSlot = FMath::RandRange(0, matchingItems.Num() - 1);
		const int32 selectedCount = FMath::RandRange(generationRules.MinItemCount, generationRules.MaxItemCount);
		if (matchingItems.IsValidIndex(selectedSlot) && selectedCount > 0) {
			const TSubclassOf<UACFItem> ItemClass = matchingItems[selectedSlot].ItemClass.LoadSynchronous();
			if (ItemClass) {
				outItem = FBaseItem(ItemClass, selectedCount);
				return true;
			}
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("No  ItemsDB! in ItemsManager!!!!- UACFItemsManagerComponent"));
	}
	return false;
}

bool UACFItemsManagerComponent::DoesSlotMatchesRule(const FACFItemGenerationRule& generationRules, const FItemGenerationSlot& item)
{
	return (
		(item.Category == generationRules.Category || UGameplayTagsManager::Get().RequestGameplayTagChildren(generationRules.Category).HasTag(item.Category))
		&& (item.Rarity == generationRules.Rarity || UGameplayTagsManager::Get().RequestGameplayTagChildren(generationRules.Rarity).HasTag(item.Rarity)));
}


void UACFItemsManagerComponent::SellItemsToVendor_Implementation(const FInventoryItem& itemTobeSold, APawn* instigator, int32 count /*= 1*/, UACFVendorComponent* vendorComp)
{
	if (!vendorComp) {
		return;
	}

	if (!vendorComp->CanPawnSellItemFromHisInventory(itemTobeSold, instigator, count)) {
		return;
	}

	UACFEquipmentComponent* equipComp = vendorComp->GetPawnEquipment(instigator);
	UACFCurrencyComponent* currencyComp = vendorComp->GetPawnCurrencyComponent(instigator);
	FItemDescriptor itemData;
	if (!UACFItemSystemFunctionLibrary::GetItemData(itemTobeSold.ItemClass, itemData)) {
		return;
	}
	const float totalCost = itemData.CurrencyValue * count * vendorComp->GetVendorPriceMultiplierOnBuy();
	if (equipComp && currencyComp) {
		// Capture the live inventory item (with UACFItem* and fragment state)
		// BEFORE RemoveItem destroys the entry.
		FInventoryItem liveItem;
		equipComp->GetItemByGuid(itemTobeSold.GetItemGuid(), liveItem);

		equipComp->RemoveItem(itemTobeSold, count);
		currencyComp->AddCurrency(totalCost);
		if (vendorComp->VendorUsesCurrency() && vendorComp->GetVendorCurrencyComp()) {
			vendorComp->GetVendorCurrencyComp()->RemoveCurrency(totalCost);
		}

		// Transfer the live item with its UACFItem* pointer intact
		FInventoryItem toBeSold = liveItem;
		toBeSold.Count = count;
		vendorComp->AddInventoryItem(toBeSold);
		OnItemSold.Broadcast(toBeSold);
	}
}

void UACFItemsManagerComponent::BuyItems_Implementation(const FInventoryItem& item, APawn* instigator, UACFVendorComponent* vendorComp)
{
	if (!vendorComp) {
		return;
	}

	if (!vendorComp->CanPawnBuyItems(item, instigator)) {
		return;
	}
	UACFEquipmentComponent* equipComp = vendorComp->GetPawnEquipment(instigator);

	BuyItemsFromVendor(vendorComp, item, equipComp);
	return;
}

void UACFItemsManagerComponent::BuyItemsFromVendor(UACFVendorComponent* vendorComp, const FInventoryItem& item, UACFEquipmentComponent* equipComp)
{
	FItemDescriptor itemToBuyDesc;
	if (vendorComp->TryGetItemDescriptor(item, itemToBuyDesc)) {
		const float totalCost = (itemToBuyDesc.CurrencyValue * item.Count * vendorComp->GetVendorPriceMultiplierOnSell());
		//UACFCurrencyComponent* currencyComp = vendorComp->GetPawnCurrencyComponent(instigator);
		if (equipComp) {
			// Capture the live UACFItem* from the vendor's inventory BEFORE removal
			FInventoryItem vendorItem;
			vendorComp->GetItemByGuid(item.GetItemGuid(), vendorItem);

			equipComp->RemoveCurrency(totalCost);

			// Use AddInventoryItem (takes FInventoryItem) so the Item* pointer
			// flows through to Internal_AddInventoryItem and gets adopted.
			FInventoryItem toTransfer = vendorItem;
			toTransfer.Count = item.Count;
			equipComp->AddInventoryItem(toTransfer);

			vendorComp->RemoveItem(item, item.Count);
			if (vendorComp->GetVendorCurrencyComp()) {
				vendorComp->GetVendorCurrencyComp()->AddCurrency(totalCost);
			}
			OnItemPurchased.Broadcast(item);
			return;
		}
	}
	return;
}


void UACFItemsManagerComponent::CraftItem_Implementation(const FACFCraftingRecipe& ItemToCraft, APawn* instigator, class UACFCraftingComponent* craftingComp)
{
	if (!craftingComp) {
		return;
	}

	if (!craftingComp->CanPawnCraftItem(ItemToCraft, instigator)) {
		return;
	}
	UACFEquipmentComponent* equipComp = craftingComp->GetPawnEquipment(instigator);

	if (!equipComp) {
		return;
	}

	craftingComp->GetPawnCurrencyComponent(instigator)->RemoveCurrency(craftingComp->GetVendorPriceMultiplierOnSell() * ItemToCraft.CraftingCost);
	equipComp->ConsumeItems(ItemToCraft.RequiredItems);
	equipComp->AddItemToInventoryByClass(ItemToCraft.OutputItem.ItemClass, ItemToCraft.OutputItem.Count);
	OnItemCrafted.Broadcast(ItemToCraft);
	return;
}

void UACFItemsManagerComponent::UpgradeItem_Implementation(const FInventoryItem& itemToUpgrade, APawn* instigator, class UACFCraftingComponent* craftingComp)
{
	if (!craftingComp) {
		return;
	}

	if (!craftingComp->CanPawnUpgradeItem(itemToUpgrade, instigator)) {
		return;
	}

	UACFEquipmentComponent* equipComp = craftingComp->GetPawnEquipment(instigator);
	UACFCurrencyComponent* currencyComp = craftingComp->GetPawnCurrencyComponent(instigator);
	if (equipComp && currencyComp) {

		FItemDescriptor itemInfo;
		if (!UACFItemSystemFunctionLibrary::GetItemData(itemToUpgrade.ItemClass, itemInfo)) {
			return;
		}

		if (itemInfo.NextLevelClass) {
			equipComp->ConsumeItems(itemInfo.RequiredItemsToUpgrade);
			currencyComp->RemoveCurrency(craftingComp->GetVendorPriceMultiplierOnSell() * itemInfo.UpgradeCurrencyCost);
			equipComp->RemoveItem(itemToUpgrade, 1);
			equipComp->AddItemToInventoryByClass(itemInfo.NextLevelClass, 1);
			OnItemUpgraded.Broadcast(itemToUpgrade);
			return;
		}
	}
	return;
}

void UACFItemsManagerComponent::DropItem_Implementation(UACFEquipmentComponent* targetComponent, const FInventoryItem& itemToDrop, int32 count /*= 1*/)
{
	if (targetComponent) {
		targetComponent->DropItem(itemToDrop, count);
	}
}

void UACFItemsManagerComponent::UseInventoryItem_Implementation(UACFEquipmentComponent* targetComponent, const FInventoryItem& itemToUse)
{
	if (targetComponent) {
		targetComponent->UseInventoryItem(itemToUse);
	}
}

void UACFItemsManagerComponent::EquipItemFromInventoryInSlot_Implementation(UACFEquipmentComponent* targetComponent, const FInventoryItem& itemToUse, const FGameplayTag& slot)
{
	if (targetComponent) {
		targetComponent->EquipItemFromInventoryInSlot(itemToUse, slot);
	}
}

void UACFItemsManagerComponent::ConstructBuildable_Implementation(APawn* instigator, class UACFBuildableComponent* buildComp)
{
	if (buildComp && instigator && buildComp->CanBeBuildByPawn(instigator)) {
		buildComp->GetPawnCurrencyComponent(instigator)->RemoveCurrency(buildComp->GetBuildingCost());
		TArray<FBaseItem> outItems;
		buildComp->GetBuildingRequirements(outItems);
		buildComp->GetPawnEquipment(instigator)->ConsumeItems(outItems);
		buildComp->SetBuildingState(EBuildableState::EBuilded);
	}
}
//
//void UACFItemsManagerComponent::ApplyAppearanceFromSkeletalMeshes(const TArray<USkeletalMeshComponent*>& SkeletalComponents, const TObjectPtr<AACFCharacter> Character) {
//	if (!IsValid(Character)) {
//		return;
//	}
//
//	USkeletalMeshComponent* MainMesh = Character->GetMesh();
//	if (!MainMesh) {
//		return;
//	}
//
//	// The character is treated as having no meshes: wipe the ones created by previous calls and the main mesh
//	TArray<USkeletalMeshComponent*> ExistingComponents;
//	Character->GetComponents<USkeletalMeshComponent>(ExistingComponents);
//	for (USkeletalMeshComponent* Existing : ExistingComponents) {
//		if (Existing && Existing->ComponentHasTag(AppearanceCopyTag)) {
//			Existing->DestroyComponent();
//		}
//	}
//	MainMesh->SetSkeletalMesh(nullptr);
//
//	auto CopyMaterials = [](const USkeletalMeshComponent* From, USkeletalMeshComponent* To) {
//		const int32 NumMaterials = From->GetNumMaterials();
//		for (int32 Idx = 0; Idx < NumMaterials; ++Idx) {
//			To->SetMaterial(Idx, From->GetMaterial(Idx));
//		}
//		};
//
//	// The main mesh must be copied first so the other parts can follow its pose
//	for (const USkeletalMeshComponent* Source : SkeletalComponents) {
//		if (!IsValid(Source) || !Source->GetSkeletalMeshAsset()) {
//			continue;
//		}
//		const ACharacter* SourceCharacter = Cast<ACharacter>(Source->GetOwner());
//		if (SourceCharacter && SourceCharacter->GetMesh() == Source) {
//			MainMesh->SetSkeletalMesh(Source->GetSkeletalMeshAsset());
//			MainMesh->SetVisibility(Source->IsVisible());
//			CopyMaterials(Source, MainMesh);
//			break;
//		}
//	}
//
//	// Every other skeletal mesh becomes a new component attached to the main mesh
//	for (const USkeletalMeshComponent* Source : SkeletalComponents) {
//		if (!IsValid(Source) || !Source->GetSkeletalMeshAsset()) {
//			continue;
//		}
//		const ACharacter* SourceCharacter = Cast<ACharacter>(Source->GetOwner());
//		if (SourceCharacter && SourceCharacter->GetMesh() == Source) {
//			continue;
//		}
//
//		USkeletalMeshComponent* NewComp = NewObject<USkeletalMeshComponent>(Character, NAME_None, RF_Transient);
//		NewComp->ComponentTags.Add(AppearanceCopyTag);
//		NewComp->SetupAttachment(MainMesh);
//		NewComp->RegisterComponent();
//		NewComp->SetSkeletalMesh(Source->GetSkeletalMeshAsset());
//		NewComp->SetVisibility(Source->IsVisible());
//		CopyMaterials(Source, NewComp);
//		if (MainMesh->GetSkeletalMeshAsset()) {
//			NewComp->SetLeaderPoseComponent(MainMesh);
//		}
//	}
//}


void UACFItemsManagerComponent::ApplyAppearance(const FEquipment& Equipment)
{
	AACFCharacter* Player = UACFMountSystemFunctionLibrary::GetLocalRiderPlayerCharacter(this);

	if (!Player || !PreviewCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyAppearance: Missing character"));
		return;
	}

	// Now copy the actual equipped items from the party member
	UACFEquipmentComponent* SourceEquipment = Player->FindComponentByClass<UACFEquipmentComponent>();
	UACFEquipmentComponent* TargetEquipment = PreviewCharacter->FindComponentByClass<UACFEquipmentComponent>();

	if (SourceEquipment && TargetEquipment)
	{
		TargetEquipment->SetAllowedWeaponTypes(SourceEquipment->GetAllowedWeaponTypes());
		TargetEquipment->SetAvailableEquipmentSlot(SourceEquipment->GetAvailableEquipmentSlot());
		// Copy all equipment from source to target
		TargetEquipment->CopyEquipmentFrom(SourceEquipment);
	}

	UE_LOG(LogTemp, Log, TEXT("Successfully applied appearance from %s to possessed pawn %s"), *Player->GetName(), *PreviewCharacter->GetName());
}

AACFCharacterPreviewStage* UACFItemsManagerComponent::Internal_SpawnLoadedPreviewStage()
{
	if (IsValid(ItemPreviewStage)) {
		return ItemPreviewStage;
	}

	UClass* LoadedClass = ItemPreviewStageClass.Get();
	if (!LoadedClass || !GetWorld()) {
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ItemPreviewStage = GetWorld()->SpawnActor<AACFCharacterPreviewStage>(LoadedClass, ItemPreviewStageTransform, SpawnParams);
	if (!IsValid(ItemPreviewStage)) {
		return nullptr;
	}

	AACFCharacter* Player = UACFMountSystemFunctionLibrary::GetLocalRiderPlayerCharacter(this);
	if (!IsValid(Player)) {
		return nullptr;
	}
	// The preview is a copy of the player's own class. It is spawned deferred so no AI controller ever takes it over
	PreviewCharacter = GetWorld()->SpawnActorDeferred<AACFCharacter>(Player->GetClass(), ItemPreviewStageTransform, GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(PreviewCharacter)) {
		return nullptr;
	}
	PreviewCharacter->AutoPossessAI = EAutoPossessAI::Disabled;
	PreviewCharacter->FinishSpawning(ItemPreviewStageTransform);
	PreviewCharacter->AttachToActor(ItemPreviewStage, FAttachmentTransformRules::KeepWorldTransform);
	// Keep the copy inert: it must not fall, collide, be hurt or die while it sits on the stage
	PreviewCharacter->SetCanBeDamaged(false);
	PreviewCharacter->SetIsImmortal(true);
	PreviewCharacter->SetActorEnableCollision(false);
	SpawnItemPreviewLight();

	USceneCaptureComponent2D* CaptureComponent = PreviewCharacter->GetComponentByClass<USceneCaptureComponent2D>();
	if (!IsValid(CaptureComponent)) {
		UE_LOG(LogTemp, Warning, TEXT("Character is missing a SceneCaptureComponent2D"));
		return ItemPreviewStage;
	}
	CaptureComponent->TextureTarget = ItemPreviewRenderTarget;
	CaptureComponent->bCaptureEveryFrame = true;
	CaptureComponent->SetTickableWhenPaused(true);

	PreviewCharacter->GetCharacterMovement()->GravityScale = 0.f;
	if (!IsValid(Player)) {
		return nullptr;
	}

	UACFEquipmentComponent* EquipmentComponent = Player->FindComponentByClass<UACFEquipmentComponent>();
	if (!IsValid(EquipmentComponent)) {
		return nullptr;
	}
	EquipmentComponent->OnEquipmentChanged.AddDynamic(this, &UACFItemsManagerComponent::ApplyAppearance);

	ApplyAppearance({});

	return ItemPreviewStage;
}

AACFCharacterPreviewStage* UACFItemsManagerComponent::SpawnItemPreviewStage()
{
	if (ItemPreviewStageClass.IsNull()) {
		return nullptr;
	}
	if (IsValid(ItemPreviewStage)) {
		return ItemPreviewStage;
	}

	if (!ItemPreviewStageClass.IsValid()) {
		if (!PreviewStreamableHandle.IsValid()) {
			PreviewStreamableHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
				ItemPreviewStageClass.ToSoftObjectPath(),
				FStreamableDelegate::CreateUObject(this, &UACFItemsManagerComponent::OnItemPreviewAssetsLoaded),
				FStreamableManager::AsyncLoadHighPriority);
		}
		return nullptr;
	}

	return Internal_SpawnLoadedPreviewStage();
}

void UACFItemsManagerComponent::OnItemPreviewAssetsLoaded()
{
	PreviewStreamableHandle.Reset();

	if (!Internal_SpawnLoadedPreviewStage()) {
		PreviewCharacter = nullptr;
		return;
	}

	TArray<ACharacter*> Characters;
	if (IsValid(PreviewCharacter)) {
		Characters.Add(PreviewCharacter);
	}

	if (Characters.Num() > 0) {
		ItemPreviewStage->PositioningCharactersOnPreviewSphere(Characters, ItemPreviewSphereRadius, ItemPreviewSpacingMultiplier);
		UpdateItemPreviewLightSettings();
	}
}

void UACFItemsManagerComponent::MoveCharacterToItemPreviewStage()
{
	if (!IsValid(ItemPreviewStage) || !IsValid(PreviewCharacter))
	{
		UE_LOG(LogTemp, Warning, TEXT("PreviewCharactersStage is null"));
		return;
	}
	TArray<ACharacter*> Characters;
	Characters.Add(PreviewCharacter);
	ItemPreviewStage->MoveCharactersOnPreviewSphere(Characters);
	UpdateItemPreviewLightSettings();
}

void UACFItemsManagerComponent::RefreshItemPreviewStage()
{
	if (!IsValid(ItemPreviewStage) || !IsValid(PreviewCharacter)) {
		UE_LOG(LogTemp, Warning, TEXT("RefreshItemPreviewStage: ItemPreviewStage is null - call SpawnItemPreviewStage/PositionCharacterOnItemPreviewStage first - UACFItemsManagerComponent"));
		return;
	}
	TArray<ACharacter*> Characters;
	Characters.Add(PreviewCharacter);
	ItemPreviewStage->PositioningCharactersOnPreviewSphere(Characters, ItemPreviewSphereRadius, ItemPreviewSpacingMultiplier);
	UpdateItemPreviewLightSettings();
}
