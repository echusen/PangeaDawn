// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ACFAnimSyncComponent.generated.h"

class UAnimationAsset;
class USkeletalMeshComponent;

USTRUCT()
struct FACFSyncedAnim
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAnimationAsset> Anim = nullptr;

	UPROPERTY()
	float StateServerTime  = 0.f;
	
	UPROPERTY()
	float Position  = 0.f;

	UPROPERTY()
	float PlayRate = 1.f;

	UPROPERTY()
	bool bLooping = true;
	
	UPROPERTY()
	bool bPlaying = true;

	UPROPERTY()
	uint8 PlayId = 0;
};

UCLASS( ClassGroup=(ACF), meta=(BlueprintSpawnableComponent) )
class CLIMBINGSYSTEM_API UACFAnimSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UACFAnimSyncComponent();

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_SyncedAnim();

	void CaptureServerState();
	void ApplyReplicatedState();
	
	float GetExpectedPosition() const;
	float GetServerTime() const;
	
private:
	UPROPERTY(ReplicatedUsing=OnRep_SyncedAnim)
	FACFSyncedAnim SyncedAnim;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> MeshComponent = nullptr;
};
