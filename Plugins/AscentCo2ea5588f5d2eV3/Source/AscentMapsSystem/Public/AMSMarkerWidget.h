// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "CoreMinimal.h"

#include "AMSMarkerWidget.generated.h"

/**
 *
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHovered, const UAMSMarkerWidget*, marker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnhovered, const UAMSMarkerWidget*, marker);

UCLASS()
class ASCENTMAPSSYSTEM_API UAMSMarkerWidget : public UUserWidget {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = AMS)
    void SetMarkerIcon(UTexture2D* icon);

    UFUNCTION(BlueprintImplementableEvent, Category = AMS)
    void SetupMarkerIcon(UAMSMapMarkerComponent* markerComp);

    UPROPERTY(BlueprintAssignable, Category = AMS)
    FOnHovered OnHovered;

    UPROPERTY(BlueprintAssignable, Category = AMS)
    FOnUnhovered OnUnhovered;

    UFUNCTION(BlueprintPure, Category = AMS)
	bool IsTracked() const { return bTracked; }

    UFUNCTION(BlueprintCallable, Category = AMS)
    void TrackMarker(bool bIsTracked); 

    UFUNCTION(BlueprintImplementableEvent, Category = ANS)
    void Rotate(float Yaw);

protected:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = AMS)
    UImage* Icon;

  

    UFUNCTION(BlueprintImplementableEvent, Category = ANS)
    void HandleHovered();

    UFUNCTION(BlueprintImplementableEvent, Category = ANS)
    void HandleUnhovered();
    
    UFUNCTION(BlueprintImplementableEvent, Category = AMS)
    void HandleTrackStatusChanged(bool isTracked);

    virtual void NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

    virtual void NativeOnMouseLeave(const FPointerEvent& MouseEvent) override;

    bool bTracked;
};
