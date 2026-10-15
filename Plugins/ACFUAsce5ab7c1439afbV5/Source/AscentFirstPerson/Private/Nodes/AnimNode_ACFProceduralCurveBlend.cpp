// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimNode_ACFProceduralCurveBlend.h"

#include "Animation/AnimTrace.h"

void FAnimNode_ACFProceduralCurveBlend::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Super::Initialize_AnyThread(Context);

	SourcePose.Initialize(Context);
	CurvesPose.Initialize(Context);
}

void FAnimNode_ACFProceduralCurveBlend::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Super::CacheBones_AnyThread(Context);

	SourcePose.CacheBones(Context);
	CurvesPose.CacheBones(Context);
}

void FAnimNode_ACFProceduralCurveBlend::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Super::Update_AnyThread(Context);

	GetEvaluateGraphExposedInputs().Execute(Context);

	SourcePose.Update(Context);

	const auto CurrentBlendAmount{ GetBlendAmount() };
	if (FAnimWeight::IsRelevant(CurrentBlendAmount))
	{
		CurvesPose.Update(Context);
	}

	TRACE_ANIM_NODE_VALUE(Context, TEXT("Blend Amount"), CurrentBlendAmount)
		TRACE_ANIM_NODE_VALUE(Context, TEXT("Blend Mode"), *StaticEnum<EACFProceduralCurveBlendMode>()->GetNameStringByValue(static_cast<int64>(GetBlendMode())));
}

void FAnimNode_ACFProceduralCurveBlend::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(CurvesBlend, !IsInGameThread())

	Super::Evaluate_AnyThread(Output);

	SourcePose.Evaluate(Output);

	const auto CurrentBlendAmount{ GetBlendAmount() };
	if (!FAnimWeight::IsRelevant(CurrentBlendAmount))
	{
		return;
	}

	auto CurvesPoseContext{ Output };
	CurvesPose.Evaluate(CurvesPoseContext);

	switch (GetBlendMode())
	{
	case EACFProceduralCurveBlendMode::Additive:
		Output.Curve.Accumulate(CurvesPoseContext.Curve, CurrentBlendAmount);
		break;

	case EACFProceduralCurveBlendMode::Combine:
		Output.Curve.Combine(CurvesPoseContext.Curve);
		break;

	case EACFProceduralCurveBlendMode::CombinePreserved:
		Output.Curve.CombinePreserved(CurvesPoseContext.Curve);
		break;

	case EACFProceduralCurveBlendMode::UseMaxValue:
		Output.Curve.UseMaxValue(CurvesPoseContext.Curve);
		break;

	case EACFProceduralCurveBlendMode::UseMinValue:
		Output.Curve.UseMinValue(CurvesPoseContext.Curve);
		break;

	case EACFProceduralCurveBlendMode::Override:
		Output.Curve.Override(CurvesPoseContext.Curve);
		break;
	}
}

void FAnimNode_ACFProceduralCurveBlend::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	TStringBuilder<256> DebugItemBuilder{ InPlace, DebugData.GetNodeName(this), TEXTVIEW(": Blend Amount: ") };

	DebugItemBuilder.Appendf(TEXT("%.2f"), GetBlendAmount());

	DebugData.AddDebugItem(FString{ DebugItemBuilder });
	SourcePose.GatherDebugData(DebugData.BranchFlow(1.0f));
	CurvesPose.GatherDebugData(DebugData.BranchFlow(GetBlendAmount()));
}

float FAnimNode_ACFProceduralCurveBlend::GetBlendAmount() const
{
	return GET_ANIM_NODE_DATA(float, BlendAmount);
}

EACFProceduralCurveBlendMode FAnimNode_ACFProceduralCurveBlend::GetBlendMode() const
{
	return GET_ANIM_NODE_DATA(EACFProceduralCurveBlendMode, BlendMode);
}
