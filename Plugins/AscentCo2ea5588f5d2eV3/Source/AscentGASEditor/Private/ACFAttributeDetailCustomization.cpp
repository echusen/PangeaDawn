
#include "ACFAttributeDetailCustomization.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ACFAttributeEditorSubsystem.h"
#include "DetailWidgetRow.h"


TSharedRef<IPropertyTypeCustomization> FACFAttributePropertyCustomization::MakeInstance()
{
    return MakeShared<FACFAttributePropertyCustomization>();
}

void FACFAttributePropertyCustomization::CustomizeHeader(
    TSharedRef<IPropertyHandle> PropertyHandle,
    FDetailWidgetRow& HeaderRow,
    IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    AttributePropertyHandle = PropertyHandle;
    
    UACFAttributeEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UACFAttributeEditorSubsystem>();
    if (!Subsystem)
        return;
    
    AttributesList = Subsystem->GetAttributesList();
    
    // Get current value
    void* ValuePtr = nullptr;
    PropertyHandle->GetValueData(ValuePtr);
    
    FString CurrentAttributeString = "None";
    if (ValuePtr)
    {
        FGameplayAttribute* GameplayAttr = static_cast<FGameplayAttribute*>(ValuePtr);
        CurrentAttributeString = Subsystem->AttributeToString(*GameplayAttr);
    }
    
    // Find current selection
    CurrentSelection = AttributesList[0]; // Default to "None"
    for (const auto& Item : AttributesList)
    {
        if (*Item == CurrentAttributeString)
        {
            CurrentSelection = Item;
            break;
        }
    }
    
    HeaderRow
    .NameContent()
    [
        PropertyHandle->CreatePropertyNameWidget()
    ]
    .ValueContent()
    .MinDesiredWidth(200.0f)
    [
        SAssignNew(ComboBox, SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&AttributesList)
        .OnSelectionChanged(this, &FACFAttributePropertyCustomization::OnAttributeSelected)
        .OnGenerateWidget(this, &FACFAttributePropertyCustomization::GenerateAttributeWidget)
        .InitiallySelectedItem(CurrentSelection)
        [
            SNew(STextBlock)
            .Text(this, &FACFAttributePropertyCustomization::GetSelectedAttributeText)
        ]
    ];
}

void FACFAttributePropertyCustomization::OnAttributeSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (!NewSelection.IsValid() || !AttributePropertyHandle.IsValid())
        return;
    
    CurrentSelection = NewSelection;
    
    UACFAttributeEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UACFAttributeEditorSubsystem>();
    if (!Subsystem)
        return;
    
    FGameplayAttribute NewAttribute = Subsystem->StringToAttribute(*NewSelection);
    
    AttributePropertyHandle->NotifyPreChange();
    
    void* ValuePtr = nullptr;
    AttributePropertyHandle->GetValueData(ValuePtr);
    if (ValuePtr)
    {
        *static_cast<FGameplayAttribute*>(ValuePtr) = NewAttribute;
    }
    
    AttributePropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

TSharedRef<SWidget> FACFAttributePropertyCustomization::GenerateAttributeWidget(TSharedPtr<FString> Item)
{
    return SNew(STextBlock).Text(FText::FromString(*Item));
}

FText FACFAttributePropertyCustomization::GetSelectedAttributeText() const
{
    return CurrentSelection.IsValid() ? FText::FromString(*CurrentSelection) : FText::FromString("None");
}
