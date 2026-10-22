#include "Components/MiningSiteComponent.h"

#include "DataAssets/MiningSiteDefinition.h"
#include "GameFramework/Actor.h"

void UMiningSiteComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsSaveGame() && Ar.IsSaving())
	{
		// SaveGame archives normally omit values equal to the archetype. In particular
		// version 1 matches our CDO, but a missing version means legacy on load. Write
		// a complete tagged snapshot, including zero/default values for reused actors.
		// Restore the caller's delta policy; other actors/components share this archive.
		const bool bWasNoDelta = !Ar.DoDelta();
		Ar.ArNoDelta = true;
		Super::Serialize(Ar);
		Ar.ArNoDelta = bWasNoDelta;
		return;
	}
	if (Ar.IsSaveGame() && Ar.IsLoading())
	{
		// Missing tags in legacy records must not retain a previous load's state.
		MiningTimingVersion = 0;
		ProductionRemainder = 0.0;
		ShipmentCooldownRemaining = 0.0;
		MiningWorkforceVersion = 0;
		WorkerVacancies = 0;
		MiningDayElapsedSeconds = 0.0;
		CompletedMiningDays = 0;
		WorkerRiskSeed = 0;
		bClockInitialized = false;
		bAwaitingTimingRestore = true;
	}
	Super::Serialize(Ar);
}

bool UMiningSiteComponent::IsTimingReady() const
{
	return HasAuthority() && bEstablished && !bAwaitingTimingRestore && MiningTimingVersion == 1 && MiningWorkforceVersion == 1;
}

double UMiningSiteComponent::GetConfiguredShipmentInterval() const
{
	FMiningSiteLevelDefinition Definition;
	return GetCurrentLevelDefinition(Definition) && Definition.bShipmentUnlocked &&
		FMath::IsFinite(Definition.ShipmentIntervalSeconds)
		? FMath::Max(0.0, double(Definition.ShipmentIntervalSeconds)) : 0.0;
}

double UMiningSiteComponent::GetShipmentCooldownRemaining() const
{
	// Clients receive a snapshot, not a timestamp in the server's clock domain.
	if (!HasAuthority() || !bClockInitialized || bAwaitingTimingRestore) return ShipmentCooldownRemaining;
	const double Now = GetWorldSeconds();
	const double Elapsed = FMath::IsFinite(Now) ? FMath::Max(0.0, Now - LastAccountingWorldSeconds) : 0.0;
	return FMath::Max(0.0, ShipmentCooldownRemaining - Elapsed);
}

void UMiningSiteComponent::AccountElapsedTime()
{
	const double Now = GetWorldSeconds();
	if (!FMath::IsFinite(Now)) return;
	const double Elapsed = bClockInitialized ? FMath::Max(0.0, Now - LastAccountingWorldSeconds) : 0.0;
	// Commit the clock/cooldown before storage delegates can call back into us.
	LastAccountingWorldSeconds = Now;
	bClockInitialized = true;
	ShipmentCooldownRemaining = FMath::Max(0.0, ShipmentCooldownRemaining - Elapsed);
	AdvanceProduction(Elapsed);
}

void UMiningSiteComponent::OnComponentSaved()
{
	if (!IsTimingReady() || bTimingOperationInProgress || bUpgradeInProgress) return;
	TGuardValue<bool> Guard(bTimingOperationInProgress, true);
	AccountElapsedTime(); // Settle due online day-end losses too; never dispatch a shipment during save.
}

void UMiningSiteComponent::OnComponentLoaded()
{
	if (!HasAuthority() || !bAwaitingTimingRestore) return; // Repeated callbacks are no-ops.
	if (MiningTimingVersion > 1 || MiningTimingVersion < 0)
	{
		UE_LOG(LogPangeaMiningSite, Error, TEXT("Unsupported mining timing version %d on %s; automation remains paused."),
			MiningTimingVersion, *GetNameSafe(GetOwner()));
		return;
	}
	if (MiningWorkforceVersion < 0 || MiningWorkforceVersion > 1)
	{
		UE_LOG(LogPangeaMiningSite, Error, TEXT("Unsupported mining workforce version %d on %s; automation remains paused."),
			MiningWorkforceVersion, *GetNameSafe(GetOwner()));
		return;
	}
	if (MiningWorkforceVersion == 0)
	{
		// Pre-workforce saves had no losses: migrate to a full crew and a fresh ONLINE day.
		WorkerVacancies = 0;
		MiningDayElapsedSeconds = 0.0;
		CompletedMiningDays = 0;
		WorkerRiskSeed = FMath::Rand();
	}
	MiningWorkforceVersion = 1;
	FMiningSiteLevelDefinition LoadedLevel;
	WorkerVacancies = GetCurrentLevelDefinition(LoadedLevel) ? FMath::Clamp(WorkerVacancies, 0, FMath::Max(0, LoadedLevel.WorkerCount)) : 0;
	CompletedMiningDays = FMath::Max(int64(0), CompletedMiningDays);
	const double DayLength = SiteDefinition ? double(SiteDefinition->SimulatedDaySeconds) : 0.0;
	MiningDayElapsedSeconds = FMath::IsFinite(MiningDayElapsedSeconds) && MiningDayElapsedSeconds >= 0.0 && MiningDayElapsedSeconds < DayLength
		? MiningDayElapsedSeconds : 0.0;
	if (MiningTimingVersion == 0)
	{
		// Old saves contain no fractional output or portable cooldown. Do not invent
		// offline production, nor mistake a previous session's timestamp for this one.
		ProductionRemainder = 0.0;
		ShipmentCooldownRemaining = GetConfiguredShipmentInterval();
		UE_LOG(LogPangeaMiningSite, Warning, TEXT("Legacy mining timing on %s: resuming with zero fractional output and a full shipment cooldown."),
			*GetNameSafe(GetOwner()));
	}
	ProductionRemainder = FMath::IsFinite(ProductionRemainder) && ProductionRemainder >= 0.0 && ProductionRemainder < 1.0
		? ProductionRemainder : 0.0;
	ShipmentCooldownRemaining = FMath::IsFinite(ShipmentCooldownRemaining)
		? FMath::Clamp(ShipmentCooldownRemaining, 0.0, GetConfiguredShipmentInterval()) : GetConfiguredShipmentInterval();
	MiningTimingVersion = 1;
	LastAccountingWorldSeconds = GetWorldSeconds();
	bClockInitialized = FMath::IsFinite(LastAccountingWorldSeconds);
	bAwaitingTimingRestore = false;
	GetOwner()->ForceNetUpdate();
}
