#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Styling/AppStyle.h"
#include "IPropertyTypeCustomization.h"


class ASCENTGASEDITOR_API FACFAttributePropertyCustomization : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance();
    
    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, 
        class FDetailWidgetRow& HeaderRow, 
        IPropertyTypeCustomizationUtils& CustomizationUtils) override;
        
    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
        class IDetailChildrenBuilder& ChildBuilder,
        IPropertyTypeCustomizationUtils& CustomizationUtils) override {}

private:
    TSharedPtr<IPropertyHandle> AttributePropertyHandle;
    TSharedPtr<class SComboBox<TSharedPtr<FString>>> ComboBox;
    TArray<TSharedPtr<FString>> AttributesList;
    TSharedPtr<FString> CurrentSelection;
    
    void OnAttributeSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    TSharedRef<class SWidget> GenerateAttributeWidget(TSharedPtr<FString> Item);
    FText GetSelectedAttributeText() const;
};