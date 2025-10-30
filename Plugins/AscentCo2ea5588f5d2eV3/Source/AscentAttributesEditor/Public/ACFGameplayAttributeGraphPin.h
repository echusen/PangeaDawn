// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "AttributeSet.h"
#include "Widgets/Input/SComboBox.h"


// Extends default FGameplayAttribute pin to include ACF attributes
class SACFGameplayAttributeGraphPin : public SGraphPin
{
public:
    SLATE_BEGIN_ARGS(SACFGameplayAttributeGraphPin) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
    virtual TSharedRef<SWidget> GetDefaultValueWidget() override;
    
private:
    void OnAttributeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    bool GetDefaultValueIsEnabled() const;
    FText GetCurrentSelection() const;
    
    TArray<TSharedPtr<FString>> AttributeOptions;
    FString SelectedAttributeString;
};

class FACFGameplayAttributeGraphPanelPinFactory : public FGraphPanelPinFactory
{
public:
    virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override;
};



