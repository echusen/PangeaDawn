// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.


#include "ItemActors/ACFMeleeWeaponActor.h"
#include <Particles/ParticleSystemComponent.h>
#include "ACMCollisionManagerComponent.h"
#include <Components/SkeletalMeshComponent.h>
#include <Kismet/GameplayStatics.h>
#include <Engine/World.h>
#include <Sound/SoundCue.h>
#include <Logging.h>

void AACFMeleeWeaponActor::HandleAttackHit(const FHitResult& HitResult)
{
	OnWeaponHit.Broadcast(HitResult);
}


void AACFMeleeWeaponActor::OnWeaponUnsheathed_Implementation()
{
	Super::OnWeaponUnsheathed_Implementation();
	if (CollisionComp) {
		if (!CollisionComp->OnCollisionDetected.IsAlreadyBound(this, &AACFMeleeWeaponActor::HandleAttackHit))
		{
			CollisionComp->OnCollisionDetected.AddDynamic(this, &AACFMeleeWeaponActor::HandleAttackHit);
		}
	}
	else {
		UE_LOG(ACFInventoryLog, Warning, TEXT("NO  COLLISION MANAGER ON WEAPON"));
	}
}

void AACFMeleeWeaponActor::OnWeaponSheathed_Implementation()
{
	Super::OnWeaponSheathed_Implementation();
	if (CollisionComp)
	{
		CollisionComp->OnCollisionDetected.RemoveDynamic(this, &AACFMeleeWeaponActor::HandleAttackHit);
	}
}


AACFMeleeWeaponActor::AACFMeleeWeaponActor()
{

	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

}

void AACFMeleeWeaponActor::BeginPlay()
{
	Super::BeginPlay();

}


void AACFMeleeWeaponActor::StartWeaponSwing(const TArray<FName>& traceChannels)
{
	if (CollisionComp) {
		if (traceChannels.Num() == 0) {
			CollisionComp->StartAllTraces();
		}
		else{
			for (const auto& channel : traceChannels) {
				CollisionComp->StartSingleTrace(channel);
			}
		}
	}
}


void AACFMeleeWeaponActor::StopWeaponSwing()
{
	if (CollisionComp) {
		CollisionComp->StopAllTraces();
	}
}

bool AACFMeleeWeaponActor::GetFirstPersonDefaultCollisionPoint_Implementation(FACFFirstPersonItemPoint& OutPoint) const
{
	// The damage traces already end at the blade tip, so most melee weapons need no collision point of their own.
	if (!CollisionComp) {
		return false;
	}

	// The farthest trace end from the handle, while the mesh is loaded; the first one otherwise, resolved once it is.
	FName tipSocket = NAME_None;
	float tipDistance = -1.f;
	for (const TPair<FName, FTraceInfo>& trace : CollisionComp->GetDamageTraces()) {
		const FName endSocket = trace.Value.EndSocket;
		if (endSocket.IsNone()) {
			continue;
		}

		if (tipSocket.IsNone()) {
			tipSocket = endSocket;
		}

		if (Mesh && Mesh->DoesSocketExist(endSocket)) {
			const float distance = Mesh->GetSocketTransform(endSocket, RTS_Component).GetLocation().Size();
			if (distance > tipDistance) {
				tipDistance = distance;
				tipSocket = endSocket;
			}
		}
	}

	OutPoint = FACFFirstPersonItemPoint();
	OutPoint.SocketName = tipSocket;
	return OutPoint.IsSet();
}
