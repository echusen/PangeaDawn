// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/Function.h"
#include "ACFFreeClimbTypes.h"
#include "ACFFreeClimbableWorldSubsystem.generated.h"

class AActor;
class UACFFreeClimbableComponent;
struct FCollisionQueryParams;

UCLASS()
class CLIMBINGSYSTEM_API UACFFreeClimbableWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	void RegisterClimbable(UACFFreeClimbableComponent* Climbable);
	void UnregisterClimbable(UACFFreeClimbableComponent* Climbable);
	void UpdateMemoryStats();
	
	bool QuerySurface(const FACFSurfaceQuery& Query, const AActor* IgnoredActor, FACFSurfaceHitResult& OutHit) const;
	bool QuerySurface(const FACFSurfaceQuery& Query, const AActor* IgnoredActor, FACFSurfaceHitResult& OutHit, TFunctionRef<bool(const AActor*)> ShouldAcceptActor) const;
	bool SweepSphereSurface(const FACFSurfaceSphereSweepQuery& Query, const AActor* IgnoredActor, FACFSurfaceHitResult& OutHit) const;
	bool SweepSphereSurface(const FACFSurfaceSphereSweepQuery& Query, const AActor* IgnoredActor, FACFSurfaceHitResult& OutHit, TFunctionRef<bool(const AActor*)> ShouldAcceptActor) const;
	bool SweepSphereSurfaceMulti(const FACFSurfaceSphereSweepQuery& Query, const AActor* IgnoredActor, TArray<FACFSurfaceHitResult>& OutHits) const;
	bool SweepSphereSurfaceMulti(const FACFSurfaceSphereSweepQuery& Query, const AActor* IgnoredActor, TArray<FACFSurfaceHitResult>& OutHits, TFunctionRef<bool(const AActor*)> ShouldAcceptActor) const;
	bool SweepCapsuleSurface(const FACFSurfaceCapsuleSweepQuery& Query, const AActor* IgnoredActor, FACFSurfaceCapsuleHitResult& OutHit) const;
	bool SweepCapsuleSurface(const FACFSurfaceCapsuleSweepQuery& Query, const AActor* IgnoredActor, FACFSurfaceCapsuleHitResult& OutHit, TFunctionRef<bool(const AActor*)> ShouldAcceptActor) const;
	
private:
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UACFFreeClimbableComponent>> Climbables;

	uint64 LastMemoryStatsFrame = MAX_uint64;
};
