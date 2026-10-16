#include "UI/PangeaFootprintFocusWidget.h"

#include "Rendering/DrawElements.h"
#include "AUTUIFunctionLibrary.h"

void UPangeaFootprintFocusWidget::SetFocusState(const bool bInHasTarget, const float InProgress)
{
	bHasTarget = bInHasTarget;
	Progress = FMath::Clamp(InProgress, 0.f, 1.f);
	SetVisibility(bHasTarget ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UPangeaFootprintFocusWidget::SetRingCenter(const FVector2D InRingCenter)
{
	RingCenter = InRingCenter;
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UPangeaFootprintFocusWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 ResultLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bHasTarget)
	{
		return ResultLayer;
	}

	const FVector2D Center = RingCenter.IsNearlyZero() ? AllottedGeometry.GetLocalSize() * 0.5f : RingCenter;
	FAUTBarStyle Theme;
	const bool bHasTheme = UAUTUIFunctionLibrary::TryGetBarStyle(FGameplayTag::RequestGameplayTag(TEXT("UIStyle.Bar.Stamina")), Theme);
	const FLinearColor BackTint = bHasTheme ? Theme.BarStyle.BackgroundImage.TintColor.GetSpecifiedColor() : BackgroundColor;
	const FLinearColor FillTint = bHasTheme ? Theme.BarStyle.FillImage.TintColor.GetSpecifiedColor() : ProgressColor;
	const int32 ClampedSegmentCount = FMath::Clamp(SegmentCount, 12, 128);

	TArray<FVector2D> BackgroundPoints;
	BackgroundPoints.Reserve(ClampedSegmentCount + 1);
	for (int32 Index = 0; Index <= ClampedSegmentCount; ++Index)
	{
		const float Angle = (static_cast<float>(Index) / static_cast<float>(ClampedSegmentCount)) * 2.f * PI - HALF_PI;
		BackgroundPoints.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius);
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		ResultLayer + 1,
		AllottedGeometry.ToPaintGeometry(),
		BackgroundPoints,
		ESlateDrawEffect::None,
		BackTint,
		true,
		RingThickness);

	const int32 ProgressSegments = FMath::Clamp(FMath::CeilToInt(ClampedSegmentCount * Progress), 0, ClampedSegmentCount);
	if (ProgressSegments > 0)
	{
		TArray<FVector2D> ProgressPoints;
		ProgressPoints.Reserve(ProgressSegments + 1);
		const float ProgressAngle = Progress >= 0.999f ? (2.f * PI + 0.04f) : (2.f * PI * Progress);
		for (int32 Index = 0; Index <= ProgressSegments; ++Index)
		{
			const float Angle = (static_cast<float>(Index) / static_cast<float>(ProgressSegments)) * ProgressAngle - HALF_PI;
			ProgressPoints.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius);
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			ResultLayer + 2,
			AllottedGeometry.ToPaintGeometry(),
			ProgressPoints,
			ESlateDrawEffect::None,
			FillTint,
			true,
			RingThickness);
	}

	return ResultLayer + 2;
}
