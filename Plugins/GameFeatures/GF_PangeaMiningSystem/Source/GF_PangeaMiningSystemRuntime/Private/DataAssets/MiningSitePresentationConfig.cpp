#include "DataAssets/MiningSitePresentationConfig.h"

#include "Misc/DataValidation.h"
#include "ACFAIController.h"

const FMiningPresentationRoleConfig* UMiningSitePresentationConfig::FindRoleConfig(const EMiningPresentationRole Role) const
{
	switch (Role)
	{
	case EMiningPresentationRole::Worker:
		return &WorkerRole;
	case EMiningPresentationRole::Guard:
		return &GuardRole;
	case EMiningPresentationRole::Courier:
		return &CourierRole;
	default:
		return nullptr;
	}
}

#if WITH_EDITOR
EDataValidationResult UMiningSitePresentationConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;
	if (bUseACFRoutineJobs)
	{
		if (!FMath::IsFinite(NavigationTimeoutSeconds) || NavigationTimeoutSeconds < 1.0f || !FMath::IsFinite(BlockedRetryDelaySeconds) || BlockedRetryDelaySeconds < 0.1f)
		{
			Context.AddError(FText::FromString(TEXT("Routine navigation timeout must be >= 1s and retry delay >= 0.1s (finite values).")));
			Result = EDataValidationResult::Invalid;
		}
		const UClass* ControllerClass = RoutineControllerClass.LoadSynchronous();
		const AACFAIController* Defaults = ControllerClass ? Cast<AACFAIController>(ControllerClass->GetDefaultObject()) : nullptr;
		if (!Defaults || !Defaults->GetBehaviorTree() || !Defaults->GetDefaultAIState().MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("AIState.Routine"))))
		{
			Context.AddError(FText::FromString(TEXT("Routine jobs need an ACF controller with BehaviorTree and AIState.Routine default.")));
			Result = EDataValidationResult::Invalid;
		}
	}

	const auto ValidateRole =
		[&Context, &Result](const FMiningPresentationRoleConfig& RoleConfig, const EMiningPresentationRole ExpectedRole, const TCHAR* RoleName)
	{
		if (RoleConfig.Role != ExpectedRole)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s role config has an unexpected role enum."), RoleName)));
			Result = EDataValidationResult::Invalid;
		}

		if (RoleConfig.InteractionDuration < 0.0f)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s interaction duration must be >= 0."), RoleName)));
			Result = EDataValidationResult::Invalid;
		}

		if (RoleConfig.InteractionDurationVariance < 0.0f)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s interaction duration variance must be >= 0."), RoleName)));
			Result = EDataValidationResult::Invalid;
		}

		if (RoleConfig.TravelSpeed < 0.0f)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s travel speed must be >= 0."), RoleName)));
			Result = EDataValidationResult::Invalid;
		}

		for (int32 Index = 0; Index < RoleConfig.Stations.Num(); ++Index)
		{
			const FMiningPresentationStation& Station = RoleConfig.Stations[Index];
			if (Station.PrimaryMarkerName.IsNone())
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("%s station %d is missing a primary marker name."), RoleName, Index)));
				Result = EDataValidationResult::Invalid;
			}

			if (Station.SecondaryMarkerName.IsNone())
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("%s station %d is missing a secondary marker."), RoleName, Index)));
				Result = EDataValidationResult::Invalid;
			}
		}
	};

	ValidateRole(WorkerRole, EMiningPresentationRole::Worker, TEXT("Worker"));
	ValidateRole(GuardRole, EMiningPresentationRole::Guard, TEXT("Guard"));
	ValidateRole(CourierRole, EMiningPresentationRole::Courier, TEXT("Courier"));
	return Result;
}
#endif
