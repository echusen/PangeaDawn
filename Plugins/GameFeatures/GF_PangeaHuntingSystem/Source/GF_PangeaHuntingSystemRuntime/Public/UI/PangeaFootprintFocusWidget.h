#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PangeaFootprintFocusWidget.generated.h"

/**
 * HUD ring that fills while the player keeps the crosshair on an unidentified footprint.
 * Draws itself in NativePaint so it needs no designer-made widget tree.
 */
UCLASS(BlueprintType, Blueprintable)
class GF_PANGEAHUNTINGSYSTEMRUNTIME_API UPangeaFootprintFocusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows or hides the ring and sets how far it is filled. Progress is clamped to the 0 to 1 range. */
	UFUNCTION(BlueprintCallable, Category = "Hunting|Focus")
	void SetFocusState(bool bInHasTarget, float InProgress);

	/** Sets the ring center in local widget space. A zero vector centers the ring in the widget. */
	UFUNCTION(BlueprintCallable, Category = "Hunting|Focus")
	void SetRingCenter(FVector2D InRingCenter);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/** Radius of the ring in local widget units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Focus")
	float RingRadius = 34.f;

	/** Line thickness of the ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Focus")
	float RingThickness = 3.f;

	/** Number of line segments in the full ring. Clamped to the 12 to 128 range when painting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Focus")
	int32 SegmentCount = 48;

	/** Color of the full background ring, used when the stamina bar UI style is not available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Focus")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.95f, 0.45f, 0.18f);

	/** Color of the filled progress arc, used when the stamina bar UI style is not available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Focus")
	FLinearColor ProgressColor = FLinearColor(0.05f, 0.95f, 0.45f, 0.95f);

private:
	bool bHasTarget = false;
	float Progress = 0.f;
	FVector2D RingCenter = FVector2D::ZeroVector;
};
