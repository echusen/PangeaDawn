// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Components/RichTextBlock.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "AUTRichTextBlock.generated.h"

/**
 *
 */
UCLASS()
class ASCENTUITOOLS_API UAUTRichTextBlock : public URichTextBlock {
	GENERATED_BODY()

public:
	/** Applies the text style the current theme maps to inStyleTag. */
	UFUNCTION(BlueprintCallable, Category = AUT)
	void SetStyle(UPARAM(meta = (Categories = "UIStyle.Text")) FGameplayTag inStyleTag);

	UFUNCTION(BlueprintCallable, Category = AUT)
	void UpdateStyle();

	UFUNCTION(BlueprintPure, Category = AUT)
	FGameplayTag GetStyleTag() const { return StyleTag; }

	UFUNCTION(BlueprintCallable, Category = AUT)
	void SetFontSize(int32 inFontSize);

protected:
	/** Text style this block asks the current theme for. */
	UPROPERTY(EditAnywhere, Category = AUT, meta = (Categories = "UIStyle.Text"))
	FGameplayTag StyleTag;

	UPROPERTY(EditAnywhere, Category = AUT)
	int32 TextSize = 16;

	UPROPERTY(EditAnywhere, Category = AUT)
	bool bUseStyle = true;

	virtual void SynchronizeProperties() override;
	virtual void PostLoad() override;

private:
	/** Style this block used to point at by index. Turned into StyleTag on load, never edited again. */
	UPROPERTY()
	int32 StyleIndex = 0;
};
