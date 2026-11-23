// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "ACFDialogueSplineFollowComponent.generated.h"

class AController;
class APawn;
class UACFSplineFollowerComponent;
class UADSDialoguePartecipantComponent;

/**
 * Glue between spline follow-along and dialogue pause/resume.
 * Add this to the NPC pawn. It finds ADS Dialogue Participant on the pawn and
 * ACF Spline Follower on the pawn or its AI Controller, then pauses this NPC's
 * dialogues while waiting for the player and resumes them when the player catches up.
 */
UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent, DisplayName = "ACF Dialogue Spline Follow"))
class UNITSSYSTEM_API UACFDialogueSplineFollowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UACFDialogueSplineFollowComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Bind spline wait/rejoin to this NPC's dialogue participant. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category = ACF, meta = (DisplayName = "Bind Follow Spline Dialogue", Keywords = "pause resume spline wait dialogue"))
	bool BindFollowSplineDialogue();

	/** Unbind spline wait/rejoin from this NPC's dialogue participant. */
	UFUNCTION(BlueprintCallable, Category = ACF, meta = (DisplayName = "Unbind Follow Spline Dialogue"))
	void UnbindFollowSplineDialogue();

	UFUNCTION(BlueprintPure, Category = ACF)
	bool IsFollowSplineDialogueBound() const;

	UFUNCTION(BlueprintPure, Category = ACF)
	UACFSplineFollowerComponent* GetBoundSplineFollower() const { return BoundSplineFollower; }

	UFUNCTION(BlueprintPure, Category = ACF)
	UADSDialoguePartecipantComponent* GetBoundDialogueParticipant() const { return BoundDialogueParticipant; }

	/** Bind automatically on BeginPlay and when the pawn is possessed (server / authority). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	bool bAutoBind = true;

	/** Resume dialogues if follow stops or completes while the NPC was waiting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	bool bResumeDialogueWhenFollowEnds = true;

protected:
	UFUNCTION()
	void HandleStartedWaitingForTarget();

	UFUNCTION()
	void HandleTargetRejoined();

	UFUNCTION()
	void HandleFollowingStopped();

	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	UACFSplineFollowerComponent* FindSplineFollower() const;
	UADSDialoguePartecipantComponent* FindDialogueParticipant() const;
	void TryAutoBind();
	void BindControllerChanged();
	void UnbindControllerChanged();

	UPROPERTY(Transient, BlueprintReadOnly, Category = ACF)
	TObjectPtr<UACFSplineFollowerComponent> BoundSplineFollower;

	UPROPERTY(Transient, BlueprintReadOnly, Category = ACF)
	TObjectPtr<UADSDialoguePartecipantComponent> BoundDialogueParticipant;
};
