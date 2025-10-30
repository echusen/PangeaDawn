// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#include "ACFGameplayAttributeGraphPin.h"
#include "ScopedTransaction.h"
#include "ACFAttributeEditorSubsystem.h"
#include "Engine/Engine.h"
#include "AbilitySystemComponent.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"


void SACFGameplayAttributeGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget> SACFGameplayAttributeGraphPin::GetDefaultValueWidget()
{
    FString PinValue = GraphPinObj->GetDefaultAsString();
    SelectedAttributeString = "None"; // Default
    
    if (!PinValue.IsEmpty() && PinValue != "()")
    {
        FGameplayAttribute Attribute;
        if (FGameplayAttribute::StaticStruct()->ImportText(*PinValue, &Attribute, nullptr,
            EPropertyPortFlags::PPF_SerializedAsImportText, nullptr,
            FGameplayAttribute::StaticStruct()->GetName(), true))
        {
            if (UACFAttributeEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UACFAttributeEditorSubsystem>())
            {
                SelectedAttributeString = Subsystem->AttributeToString(Attribute); // Store clean name like "StatisticsSet.Mana"
            }
        }
    }
    
    // Populate attribute options
    AttributeOptions.Empty();
    AttributeOptions.Add(MakeShared<FString>("None"));
    
    // Add ACF attributes from subsystem
    if (UACFAttributeEditorSubsystem* ACFSubsystem = GEditor->GetEditorSubsystem<UACFAttributeEditorSubsystem>())
    {
        TArray<FString> ACFAttributes = ACFSubsystem->GetAllAttributeNames();
        for (const FString& ACFAttribute : ACFAttributes)
        {
            AttributeOptions.Add(MakeShared<FString>(ACFAttribute));
        }
    }
    
    // Create the ComboBox
    return SNew(SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&AttributeOptions)
        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
        {
            return SNew(STextBlock)
                .Text(FText::FromString(*Option))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9));
        })
        .OnSelectionChanged(this, &SACFGameplayAttributeGraphPin::OnAttributeChanged)
        .Content()
        [
            SNew(STextBlock)
                .Text(this, &SACFGameplayAttributeGraphPin::GetCurrentSelection)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
        ]
        .Visibility(this, &SGraphPin::GetDefaultValueVisibility)
        .IsEnabled(this, &SACFGameplayAttributeGraphPin::GetDefaultValueIsEnabled);
}

void SACFGameplayAttributeGraphPin::OnAttributeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (!NewSelection.IsValid()) return;
    
    // Store the clean name
    SelectedAttributeString = *NewSelection;
    
    // Convert to serialized for the pin
    if (UACFAttributeEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UACFAttributeEditorSubsystem>())
    {
        FGameplayAttribute NewAttribute = Subsystem->StringToAttribute(*NewSelection);
        
        FString SerializedValue;
        FGameplayAttribute::StaticStruct()->ExportText(SerializedValue, &NewAttribute, &NewAttribute,
            nullptr, EPropertyPortFlags::PPF_SerializedAsImportText, nullptr);
        
        // Update only the pin with serialized value
        GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, SerializedValue);
    }
}

bool SACFGameplayAttributeGraphPin::GetDefaultValueIsEnabled() const
{
	return !GraphPinObj->bDefaultValueIsReadOnly;
}

FText SACFGameplayAttributeGraphPin::GetCurrentSelection() const
{

    if (!SelectedAttributeString.IsEmpty())
    {
        return FText::FromString(SelectedAttributeString);  // Return the actual string, not "None"!
    }
    return FText::FromString("None");

	/*
	if (!SelectedAttributeString.IsEmpty())
	{
		FGameplayAttribute Attribute;
		if (FGameplayAttribute::StaticStruct()->ImportText(*SelectedAttributeString, &Attribute, nullptr,
			EPropertyPortFlags::PPF_SerializedAsImportText, GError,
			FGameplayAttribute::StaticStruct()->GetName(), true))
		{
			if (UACFAttributeEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UACFAttributeEditorSubsystem>())
			{
				FString CleanName = Subsystem->AttributeToString(Attribute);
				return FText::FromString(CleanName);
			}
		}
	}

	return FText::FromString("None");*/

}

TSharedPtr<class SGraphPin> FACFGameplayAttributeGraphPanelPinFactory::CreatePin(class UEdGraphPin* InPin) const
{
	if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (InPin->PinType.PinSubCategoryObject == FGameplayAttribute::StaticStruct())
		{
			UE_LOG(LogTemp, Warning, TEXT("Found FGameplayAttribute pin - creating custom widget"));
			return SNew(SACFGameplayAttributeGraphPin, InPin);
		}
	}

	return nullptr;
}
