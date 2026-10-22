#include "Components/MiningSiteComponent.h"

#include "DataAssets/MiningSiteDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

int32 UMiningSiteComponent::GetLivingWorkerCount() const
{
	FMiningSiteLevelDefinition Level;
	return bEstablished && GetCurrentLevelDefinition(Level) ? FMath::Max(0, FMath::Max(0, Level.WorkerCount) - WorkerVacancies) : 0;
}

bool UMiningSiteComponent::IsSiteAttended() const
{
	const UWorld* World = GetWorld();
	if (!World || !GetOwner() || !SiteDefinition || !FMath::IsFinite(SiteDefinition->WorkerAttendanceRadius)) return false;
	const double Radius = FMath::Max(1.0, double(SiteDefinition->WorkerAttendanceRadius));
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (IsValid(Pawn) && FVector::DistSquared(Pawn->GetActorLocation(), GetOwner()->GetActorLocation()) <= Radius * Radius) return true;
	}
	return false;
}

int32 UMiningSiteComponent::AdvanceProduction(const double ElapsedSeconds)
{
	if (!FMath::IsFinite(ElapsedSeconds) || ElapsedSeconds <= 0.0 || !SiteDefinition ||
		!FMath::IsFinite(SiteDefinition->SimulatedDaySeconds) || SiteDefinition->SimulatedDaySeconds < 1.f) return 0;
	const double DaySeconds = SiteDefinition->SimulatedDaySeconds;
	FMiningSiteLevelDefinition Level;
	if (!GetCurrentLevelDefinition(Level)) return 0;
	const bool bCanLoseWorkers = GetLivingWorkerCount() > 0 && FMath::IsFinite(Level.WorkerDeathChance) &&
		Level.WorkerDeathChance > 0.f && Level.WorkerDeathChance <= 1.f && !IsSiteAttended();
	if (!bCanLoseWorkers)
	{
		// O(1) for passive/attended/full-vacancy sites, even enormous explicit simulation intervals.
		const int32 Produced = ProduceSegment(ElapsedSeconds);
		const double Total = MiningDayElapsedSeconds + ElapsedSeconds;
		const double Days = FMath::FloorToDouble((Total + 1.e-9) / DaySeconds);
		MiningDayElapsedSeconds = FMath::Fmod(Total, DaySeconds);
		if (DaySeconds - MiningDayElapsedSeconds < 1.e-8) MiningDayElapsedSeconds = 0.0;
		if (Days > 0.0)
		{
			// Saturate before conversion: callers may explicitly simulate FLT_MAX seconds.
			if (Days >= double(MAX_int64 - CompletedMiningDays)) CompletedMiningDays = MAX_int64;
			else CompletedMiningDays += static_cast<int64>(Days);
			OnMiningDayEnded.Broadcast(CompletedMiningDays, 0);
		}
		return Produced;
	}
	// Reject pathological explicit multi-millennial risk simulations instead of hanging the game thread.
	if (ElapsedSeconds / DaySeconds > 10000.0)
	{
		UE_LOG(LogPangeaMiningSite, Warning, TEXT("Mining risk simulation rejected: more than 10000 online days in one request on %s."), *GetNameSafe(GetOwner()));
		return 0;
	}
	double Remaining = ElapsedSeconds;
	int64 Produced = 0;
	while (Remaining > 1.e-9)
	{
		const double Segment = FMath::Min(Remaining, DaySeconds - MiningDayElapsedSeconds);
		// Produce at the pre-loss staffing ratio. Deaths change only the following segment.
		MiningDayElapsedSeconds += Segment;
		Remaining = FMath::Max(0.0, Remaining - Segment);
		Produced += ProduceSegment(Segment);
		if (MiningDayElapsedSeconds >= DaySeconds - 1.e-8)
		{
			MiningDayElapsedSeconds = 0.0; // Commit boundary before any delegate can re-enter.
			if (CompletedMiningDays < MAX_int64) ++CompletedMiningDays;
			ResolveMiningDay();
		}
	}
	return static_cast<int32>(FMath::Min(Produced, int64(MAX_int32)));
}

void UMiningSiteComponent::ResolveMiningDay()
{
	FMiningSiteLevelDefinition Level;
	int32 Lost = 0;
	if (GetCurrentLevelDefinition(Level) && !IsSiteAttended() && FMath::IsFinite(Level.WorkerDeathChance) &&
		Level.WorkerDeathChance > 0.f && Level.WorkerDeathChance <= 1.f)
	{
		// Saved seed + monotonically saved online day => loading does not reroll an already resolved day.
		const uint64 Day = static_cast<uint64>(CompletedMiningDays);
		FRandomStream Random(static_cast<int32>(HashCombineFast(static_cast<uint32>(WorkerRiskSeed),
			HashCombineFast(static_cast<uint32>(Day), static_cast<uint32>(Day >> 32)))));
		const int32 Living = GetLivingWorkerCount();
		for (int32 Worker = 0; Worker < Living; ++Worker)
			Lost += Random.FRand() < Level.WorkerDeathChance ? 1 : 0;
		WorkerVacancies += Lost;
	}
	OnMiningDayEnded.Broadcast(CompletedMiningDays, Lost);
	if (Lost > 0)
	{
		GetOwner()->ForceNetUpdate();
		OnWorkforceChanged.Broadcast();
	}
}

bool UMiningSiteComponent::RecordWorkerDeath()
{
	if (!IsTimingReady() || bTimingOperationInProgress || bUpgradeInProgress || GetLivingWorkerCount() <= 0) return false;
	TGuardValue<bool> Guard(bTimingOperationInProgress, true);
	AccountElapsedTime();
	if (GetLivingWorkerCount() <= 0) return false;
	++WorkerVacancies;
	GetOwner()->ForceNetUpdate();
	OnWorkforceChanged.Broadcast();
	return true;
}

bool UMiningSiteComponent::CanRecruitWorker(UObject* CostContext) const
{
	FMiningSiteLevelDefinition Level;
	return bEstablished && !bAwaitingTimingRestore && MiningTimingVersion == 1 && MiningWorkforceVersion == 1 &&
		!bTimingOperationInProgress && !bUpgradeInProgress && WorkerVacancies > 0 && GetCurrentLevelDefinition(Level) &&
		Level.WorkerCount > 0 && !Level.WorkerRecruitmentCost.IsEmpty() && CanPayUpgradeCost(CostContext, Level.WorkerRecruitmentCost);
}

bool UMiningSiteComponent::RecruitWorker(UObject* CostContext)
{
	if (!HasAuthority() || !CanRecruitWorker(CostContext)) return false;
	TGuardValue<bool> Guard(bUpgradeInProgress, true);
	{
		TGuardValue<bool> TimingGuard(bTimingOperationInProgress, true);
		AccountElapsedTime(); // Never award past production to a newly hired worker.
	}
	FMiningSiteLevelDefinition Level;
	if (WorkerVacancies <= 0 || !GetCurrentLevelDefinition(Level) || Level.WorkerRecruitmentCost.IsEmpty() ||
		!CanPayUpgradeCost(CostContext, Level.WorkerRecruitmentCost) || !ConsumeUpgradeCost(CostContext, Level.WorkerRecruitmentCost)) return false;
	--WorkerVacancies;
	GetOwner()->ForceNetUpdate();
	OnWorkforceChanged.Broadcast();
	return true;
}
