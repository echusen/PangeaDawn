// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "AttributeSet.h"

#include "ACFAttributeEditorSubsystem.generated.h"


UCLASS()
class ASCENTGASEDITOR_API UACFAttributeEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	TArray<TSharedPtr<FString>> GetAttributesList();
	FGameplayAttribute StringToAttribute(const FString& AttributeString);
	FString AttributeToString(const FGameplayAttribute& Attribute);

	TArray<FString> GetAllAttributeNames() const;

	void TryReinitializeCache();

	bool IsValidAttributeName(const FString& AttributeName) const;
private:
	void RefreshCache();
	TMap<FString, FProperty*> AttributeCache;

	TMap<FString, UClass*> ClassCache;

	bool bIsCacheInitialized = false;

};
