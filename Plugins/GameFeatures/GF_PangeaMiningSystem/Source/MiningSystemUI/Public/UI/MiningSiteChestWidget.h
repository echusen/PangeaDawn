#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MiningSiteChestWidget.generated.h"

class AMiningSiteChestActor;
class APawn;
class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * Mining site chest menu. Shows what the chest's storage holds and lets the
 * interacting pawn withdraw everything at once.
 */
UCLASS()
class MININGSYSTEMUI_API UMiningSiteChestWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds the menu to a chest and the pawn using it, then refreshes the status text. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	void InitializeFromChest(AMiningSiteChestActor* InChestActor, APawn* InInteractingPawn);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildWidgetTree();
	void RefreshUI();
	void CloseMenu();
	FText BuildStatusText() const;

	UFUNCTION()
	void OnWithdrawAllClicked();

	UFUNCTION()
	void OnCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<AMiningSiteChestActor> ChestActor;

	UPROPERTY(Transient)
	TObjectPtr<APawn> InteractingPawn;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> WithdrawAllButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WithdrawAllButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CloseButtonText;
};
