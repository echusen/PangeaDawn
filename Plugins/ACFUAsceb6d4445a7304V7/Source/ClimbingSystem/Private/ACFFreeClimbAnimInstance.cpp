// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbAnimInstance.h"
#include "ACFFreeClimbableComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UACFFreeClimbAnimInstance::UACFFreeClimbAnimInstance()
{
    SetUseMainInstanceMontageEvaluationData(true);
}

void UACFFreeClimbAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    TryResolveClimbingComponent();
    RefreshRigProfile();
    RefreshRigTargets();
}

void UACFFreeClimbAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!ClimbingComponent.IsValid() && !TryResolveClimbingComponent())
    {
        return;
    }

    RefreshRigProfile();

    const FACFFreeClimbAnimData Data = ClimbingComponent->GetAnimData();
    const float InputSize = Data.ClimbInput.Size();
    ClimbState = Data.ClimbState;
    bIsMoving   = InputSize > MoveInputDeadzone;

    BlendX = (FMath::Clamp(Data.ClimbInput.X, -1.f, 1.f) + 1.f) * 50.f;
    BlendY = (FMath::Clamp(Data.ClimbInput.Y, -1.f, 1.f) + 1.f) * 50.f;

    SurfaceNormal = Data.SurfaceNormal;
    SurfacePoint  = Data.SurfacePoint;

    ClimbAnimPlayRate = Data.ClimbAnimPlayRate;
    JumpProgress = Data.JumpProgress;

    if (!bWasClimbing && ClimbState != EACFFreeClimbState::Idle)
    {
        for (FACFFreeClimbIKTarget& IKTarget : IKTargets)
        {
            if (IKTarget.bExternallyPinned)
            {
                continue;
            }
            IKTarget.bHasValidPlantTransform = false;
            IKTarget.Alpha = 0.0f;
        }
    }

    UpdateIKTargets(DeltaSeconds);
    RefreshRigTargets();

    bWasClimbing = ClimbState != EACFFreeClimbState::Idle;
}

void UACFFreeClimbAnimInstance::NativeUninitializeAnimation()
{
    if (ClimbingComponent.IsValid() && ClimbingComponent->ACFFreeClimbAnimInstance == this)
    {
        ClimbingComponent->ACFFreeClimbAnimInstance = nullptr;
    }

    ClimbingComponent.Reset();

    Super::NativeUninitializeAnimation();
}

bool UACFFreeClimbAnimInstance::TryResolveClimbingComponent()
{
    if (const AActor* Owner = GetOwningActor())
    {
        ClimbingComponent = Owner->FindComponentByClass<UACFFreeClimbComponent>();
        if (ClimbingComponent.IsValid())
        {
            ClimbingComponent->ACFFreeClimbAnimInstance = this;
        }
    }
    return ClimbingComponent.IsValid();
}

void UACFFreeClimbAnimInstance::RefreshRigProfile()
{
    UACFFreeClimbRigProfile* NewProfile = ClimbingComponent.IsValid()
        ? ClimbingComponent->GetRigProfile()
        : nullptr;

    const int32 DesiredTargetCount = static_cast<int32>(EACFFreeClimbLimb::Count);

    if (RigProfile != NewProfile ||
        IKTargets.Num() != DesiredTargetCount ||
        RigTargets.Num() != DesiredTargetCount)
    {
        RigProfile = NewProfile;
        IKTargets.SetNum(DesiredTargetCount);
        RigTargets.SetNum(DesiredTargetCount);

        for (FACFFreeClimbIKTarget& Target : IKTargets)
        {
            Target = FACFFreeClimbIKTarget{};
        }

        for (FACFFreeClimbRigTarget& Target : RigTargets)
        {
            Target = FACFFreeClimbRigTarget{};
        }
    }
}

void UACFFreeClimbAnimInstance::RefreshRigTargets()
{
    const int32 TargetCount = static_cast<int32>(EACFFreeClimbLimb::Count);
    if (RigTargets.Num() != TargetCount)
    {
        RigTargets.SetNum(TargetCount);
    }

    for (int32 Index = 0; Index < TargetCount; ++Index)
    {
        FACFFreeClimbRigTarget& RigTarget = RigTargets[Index];
        RigTarget = FACFFreeClimbRigTarget{};

        if (!RigProfile || !IKTargets.IsValidIndex(Index))
        {
            continue;
        }

        const EACFFreeClimbLimb Limb = static_cast<EACFFreeClimbLimb>(Index);
        const FACFFreeClimbLimbProfile& LimbProfile = RigProfile->GetLimbProfile(Limb);
        const FACFFreeClimbIKTarget& IKTarget = IKTargets[Index];

        RigTarget.EffectorBone = LimbProfile.EffectorBone;
        RigTarget.IKBone = LimbProfile.IKBone;
        RigTarget.TargetTransform = IKTarget.PlantTransform;
        RigTarget.Alpha = IKTarget.Alpha;
    }
}

void UACFFreeClimbAnimInstance::UpdateIKTargets(float DeltaSeconds)
{
    USkeletalMeshComponent* Mesh = GetOwningComponent();
    if (!Mesh) return;

    if (!RigProfile)
    {
        for (FACFFreeClimbIKTarget& Target : IKTargets)
        {
            Target = FACFFreeClimbIKTarget{};
        }
        return;
    }

    UACFFreeClimbableComponent* Climbable = ClimbingComponent->GetCurrentClimbable();

    for (uint8 LimbValue = 0; LimbValue < static_cast<uint8>(EACFFreeClimbLimb::Count); ++LimbValue)
    {
        const EACFFreeClimbLimb Limb = static_cast<EACFFreeClimbLimb>(LimbValue);
        const FACFFreeClimbLimbProfile& LimbProfile = RigProfile->GetLimbProfile(Limb);
        FACFFreeClimbIKTarget& Target = IKTargets[LimbValue];

        if (!LimbProfile.bEnabled)
        {
            Target = FACFFreeClimbIKTarget{};
            continue;
        }

        UpdateIKTarget(
            DeltaSeconds,
            Limb,
            LimbProfile.ContactCurveName,
            LimbProfile.SurfaceOffset,
            RigProfile->Curves.BlendInSpeed,
            RigProfile->Curves.BlendOutSpeed,
            RigProfile->Curves.TraceThreshold,
            RigProfile->Curves.LockThreshold,
            Target,
            Mesh,
            Climbable);
    }
}

void UACFFreeClimbAnimInstance::UpdateIKTarget(
    float DeltaSeconds,
    EACFFreeClimbLimb Limb,
    FName ContactCurveName,
    float SurfaceOffset,
    float BlendInSpeed,
    float BlendOutSpeed,
    float TraceThreshold,
    float LockThreshold,
    FACFFreeClimbIKTarget& IKTarget,
    const USkeletalMeshComponent* Mesh,
    const UACFFreeClimbableComponent* Climbable)
{
    const float TargetAlpha = IKTarget.bHasValidPlantTransform ? 1.0f : 0.0f;
    const float BlendSpeed = TargetAlpha > IKTarget.Alpha ? BlendInSpeed : BlendOutSpeed;
    IKTarget.Alpha = FMath::FInterpTo(IKTarget.Alpha, TargetAlpha, DeltaSeconds, BlendSpeed);

    if (!IKTarget.bExternallyPinned)
    {
        const float CurveValue = GetCurveValue(ContactCurveName);
        UpdateIKTargetState(IKTarget, CurveValue, TraceThreshold, LockThreshold);
    }

    if (!IKTarget.bExternallyPinned && IKTarget.State == ELimbIKState::Trace)
    {
        FACFFreeClimbContact Contact;

        const bool bHasContact = ClimbingComponent.IsValid() && ClimbingComponent->GetLimbContact(Limb, Contact);

        if (bHasContact && Contact.bValid)
        {
            const FACFSurfaceHitResult& Hit = Contact.SurfaceHit;

            IKTarget.Source = Hit.Source;

            if (Hit.Source == EACFSurfaceSource::Static)
            {
                IKTarget.Primitive = Hit.HitComponent;

                const FTransform ComponentWorldTransform = Hit.HitComponent->GetComponentTransform();
                IKTarget.LocalPos = ComponentWorldTransform.InverseTransformPosition(Hit.Position);
                IKTarget.LocalNormal = ComponentWorldTransform.InverseTransformVectorNoScale(Hit.Normal.GetSafeNormal());

                IKTarget.Climbable = nullptr;
            }
            else
            {
                IKTarget.Climbable = Hit.Climbable;
                IKTarget.TriangleIndex = Hit.TriangleIndex;
                IKTarget.Barycentric = Hit.Barycentric;

                IKTarget.Primitive = nullptr;
            }

            IKTarget.bHasValidPlantTransform = true;
            IKTarget.bPlanted = true;
        }
        else
        {
            IKTarget.bHasValidPlantTransform = false;
            IKTarget.bPlanted = false;
        }
    }

    if (IKTarget.bHasValidPlantTransform && IKTarget.State != ELimbIKState::Free)
    {
        FVector AnchorLocation;
        FVector AnchorNormal;

        if (IKTarget.Source == EACFSurfaceSource::Static)
        {
            UPrimitiveComponent* Prim = IKTarget.Primitive.Get();
            if (!Prim)
            {
                IKTarget.bHasValidPlantTransform = false;
                return;
            }

            const FTransform ComponentWorldTransform = Prim->GetComponentTransform();
            AnchorLocation = ComponentWorldTransform.TransformPosition(IKTarget.LocalPos);
            AnchorNormal = ComponentWorldTransform.TransformVectorNoScale(IKTarget.LocalNormal).GetSafeNormal();
        }
        else
        {
            const UACFFreeClimbableComponent* TargetClimbable =
                IKTarget.Climbable ? IKTarget.Climbable.Get() : Climbable;

            if (!TargetClimbable)
            {
                IKTarget.bHasValidPlantTransform = false;
                return;
            }

            if (!TargetClimbable->ResolveAnchor(IKTarget.TriangleIndex, IKTarget.Barycentric, AnchorLocation, AnchorNormal))
            {
                IKTarget.bHasValidPlantTransform = false;
                return;
            }
        }

        const FVector PalmForward = -AnchorNormal.GetSafeNormal();

        FVector UpHint = Mesh->GetUpVector();
        if (FMath::Abs(FVector::DotProduct(PalmForward, UpHint)) > 0.98f)
        {
            UpHint = Mesh->GetForwardVector();
        }

        const FQuat SurfaceRotation = FRotationMatrix::MakeFromXZ(PalmForward, UpHint).ToQuat();
        IKTarget.PlantTransform.SetLocation(AnchorLocation + AnchorNormal * SurfaceOffset);
        IKTarget.PlantTransform.SetRotation(SurfaceRotation);
    }
}

void UACFFreeClimbAnimInstance::UpdateIKTargetState(
    FACFFreeClimbIKTarget& IKTarget,
    float CurveValue,
    float TraceThreshold,
    float LockThreshold)
{
    if (CurveValue < TraceThreshold)
    {
        IKTarget.State = ELimbIKState::Free;
        IKTarget.bPlanted = false;
        IKTarget.bHasValidPlantTransform = false;
    }
    else if (CurveValue < LockThreshold && !IKTarget.bPlanted)
    {
        IKTarget.State = ELimbIKState::Trace;
    }
    else if (CurveValue < LockThreshold && IKTarget.bPlanted)
    {
        IKTarget.State = ELimbIKState::Release;
    }
    else if (IKTarget.bHasValidPlantTransform)
    {
        IKTarget.State = ELimbIKState::Locked;
    }
    else
    {
        IKTarget.State = ELimbIKState::Trace;
    }
}

void UACFFreeClimbAnimInstance::PinLimbToAnchor(EACFFreeClimbLimb Limb, const FACFSurfaceHitResult& Hit)
{
    const int32 LimbIndex = static_cast<int32>(Limb);
    if (!IKTargets.IsValidIndex(LimbIndex))
    {
        return;
    }

    FACFFreeClimbIKTarget& IKTarget = IKTargets[LimbIndex];
    IKTarget.Source = Hit.Source;

    if (Hit.Source == EACFSurfaceSource::Static)
    {
        IKTarget.Primitive = Hit.HitComponent;

        const FTransform ComponentWorldTransform = Hit.HitComponent->GetComponentTransform();
        IKTarget.LocalPos = ComponentWorldTransform.InverseTransformPosition(Hit.Position);
        IKTarget.LocalNormal = ComponentWorldTransform.InverseTransformVectorNoScale(Hit.Normal.GetSafeNormal());

        IKTarget.Climbable = nullptr;
    }
    else
    {
        IKTarget.Climbable = Hit.Climbable;
        IKTarget.TriangleIndex = Hit.TriangleIndex;
        IKTarget.Barycentric = Hit.Barycentric;

        IKTarget.Primitive = nullptr;
    }

    IKTarget.bHasValidPlantTransform = true;
    IKTarget.bPlanted = true;
    IKTarget.State = ELimbIKState::Locked;
    IKTarget.bExternallyPinned = true;
}

void UACFFreeClimbAnimInstance::ReleaseAllPins()
{
    for (FACFFreeClimbIKTarget& IKTarget : IKTargets)
    {
        IKTarget.bExternallyPinned = false;
    }
}
