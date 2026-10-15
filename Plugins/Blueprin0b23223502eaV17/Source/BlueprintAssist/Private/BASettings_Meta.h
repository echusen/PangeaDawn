// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistMisc/BASettingsBase.h"
#include "BASettings_Meta.generated.h"

/**
 * 
 */
UCLASS(config = EditorPerProjectUserSettings)
class BLUEPRINTASSIST_API UBASettings_Meta : public UBASettingsBase
{
	GENERATED_BODY()

public:
	UBASettings_Meta(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(config)
	int ConfigVersion;

	UPROPERTY(EditAnywhere, config, Category = "Default", meta = (FilePathFilter = "ini", RelativeToGameDir, ConfigRestartRequired = "true"))
	FFilePath CustomSettingsIniPath;

	FORCEINLINE static const UBASettings_Meta& Get() { return *GetDefault<UBASettings_Meta>(); }
	FORCEINLINE static UBASettings_Meta& GetMutable() { return *GetMutableDefault<UBASettings_Meta>(); }
};
