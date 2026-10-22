#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningLocomotionDataTest, "Pangea.ACF.Mining.WalkBlendspaceCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningLocomotionDataTest::RunTest(const FString& Parameters)
{
	UBlendSpace* Blend = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Yarin/BS_Pangea_MiningLocomotion.BS_Pangea_MiningLocomotion"));
	if (!TestNotNull(TEXT("Project-owned mining locomotion"), Blend)) return false;
	for (const float Speed : {220.f, 250.f, 320.f})
	{
		TArray<FBlendSampleData> Weights;
		int32 Triangle = INDEX_NONE;
		TestTrue(TEXT("Travel speed is covered"), Blend->GetSamplesFromBlendInput(FVector(0, Speed / 800.f, 0), Weights, Triangle, false));
		float IdleWeight = 0.f, WalkWeight = 0.f;
		double AuthoredGroundSpeed = 0.0;
		for (const FBlendSampleData& Weight : Weights)
		{
			const FBlendSample& Sample = Blend->GetBlendSamples()[Weight.SampleDataIndex];
			if (Sample.SampleValue.Y == 0.f) IdleWeight += Weight.TotalWeight;
			if (Sample.Animation && Sample.Animation->GetName().Contains(TEXT("Walk"))) WalkWeight += Weight.TotalWeight;
			if (Sample.Animation)
			{
				const double Length = Sample.Animation->GetPlayLength();
				const FTransform Root = Sample.Animation->ExtractRootMotionFromRange(0., Length, FAnimExtractContext());
				AuthoredGroundSpeed += Weight.TotalWeight * Sample.RateScale * Sample.Animation->RateScale *
					Root.GetTranslation().Size2D() / FMath::Max(Length, .001);
			}
		}
		AddInfo(FString::Printf(TEXT("Travel %.0f cm/s: idle weight %.3f, walk weight %.3f"), Speed, IdleWeight, WalkWeight));
		TestTrue(TEXT("Cruising NPCs do not blend idle into walking"), IdleWeight < .01f);
		TestTrue(TEXT("Travel uses authored walking, not a diluted jog"), WalkWeight > .5f);
		TestTrue(TEXT("Authored walk cadence matches navigation ground speed within 5 cm/s"), FMath::Abs(AuthoredGroundSpeed - Speed) < 5.0);
	}
	for (const TCHAR* Direction : {TEXT("Fwd"), TEXT("Bwd"), TEXT("Left"), TEXT("Right")})
	{
		const FString Name = FString::Printf(TEXT("MM_Unarmed_Walk_%s"), Direction);
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *(TEXT("/Game/FullSample/Animations/Locomotion/Unarmed/") + Name + TEXT(".") + Name));
		if (!TestNotNull(TEXT("Walking source clip"), Sequence)) continue;
		const double Length = Sequence->GetPlayLength();
		const FTransform Root = Sequence->ExtractRootMotionFromRange(0., Length, FAnimExtractContext());
		AddInfo(FString::Printf(TEXT("%s: cycle %.3fs, source root speed %.1f cm/s, RateScale %.3f"), *Name, Length, Root.GetTranslation().Size2D() / FMath::Max(Length, .001), Sequence->RateScale));
	}
	return true;
}
#endif
