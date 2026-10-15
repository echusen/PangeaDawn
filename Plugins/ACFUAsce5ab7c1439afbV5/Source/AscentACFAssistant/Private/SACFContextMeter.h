// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.
#pragma once

#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElementTypes.h"

class SACFContextMeter : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SACFContextMeter) : _Fraction(0.f) {}
  SLATE_ATTRIBUTE(float, Fraction)
 SLATE_END_ARGS()

 void Construct(const FArguments& Args)
 {
  Fraction = Args._Fraction;
  Track.Reserve(Segments + 1);
  Arc.Reserve(Segments + 1);
 }
 virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(24.f, 24.f); }
 virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
  int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
 {
  const float Raw = Fraction.Get(0.f);
  const float Value = FMath::IsFinite(Raw) ? FMath::Clamp(Raw, 0.f, 1.f) : 0.f;
  const FVector2D Size = Geometry.GetLocalSize();
  if (Size != CachedSize)
  {
   CachedSize = Size;
   Track.Reset();
   const FVector2D Center = Size * 0.5;
   const double Radius = FMath::Max(0.0, FMath::Min(Size.X, Size.Y) * 0.5 - 2.0);
   for (int32 Index = 0; Index <= Segments; ++Index)
   {
    const double Angle = 2.0 * UE_PI * Index / Segments - UE_PI * 0.5;
    Track.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
   }
   CachedFraction = -1.f;
  }
  if (Value != CachedFraction)
  {
   CachedFraction = Value;
   Arc.Reset();
   if (Value > 0.f)
   {
    const float Steps = Value * Segments;
    const int32 Whole = FMath::FloorToInt(Steps);
    for (int32 Index = 0; Index <= Whole; ++Index) { Arc.Add(Track[Index]); }
    if (Whole < Segments)
    {
     const double Angle = 2.0 * UE_PI * Value - UE_PI * 0.5;
     const double Radius = FMath::Max(0.0, FMath::Min(Size.X, Size.Y) * 0.5 - 2.0);
     Arc.Add(Size * 0.5 + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
    }
   }
  }
  const ESlateDrawEffect Effects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
  const FLinearColor Tint = Style.GetColorAndOpacityTint();
  FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Track, Effects, FLinearColor::FromSRGBColor(FColor(68, 70, 78)) * Tint, true, 2.f);
  if (Arc.Num() > 1)
  {
   const FLinearColor Color = Value >= 0.9f ? FLinearColor(0.9f, 0.35f, 0.3f)
    : Value >= 0.75f ? FLinearColor(0.9f, 0.65f, 0.3f) : FLinearColor::FromSRGBColor(FColor(182, 176, 154));
   FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Arc, Effects, Color * Tint, true, 2.f);
  }
  return Layer + 1;
 }
private:
 static constexpr int32 Segments = 48;
 TAttribute<float> Fraction;
 mutable FVector2D CachedSize = FVector2D(-1.f, -1.f);
 mutable float CachedFraction = -1.f;
 mutable TArray<FVector2D> Track;
 mutable TArray<FVector2D> Arc;
};
