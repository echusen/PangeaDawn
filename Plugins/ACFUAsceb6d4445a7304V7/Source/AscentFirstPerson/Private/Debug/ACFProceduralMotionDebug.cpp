// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Debug/ACFProceduralMotionDebug.h"

#if ACF_FP_DEBUG_CAPTURE

#include "DisplayDebugHelpers.h"
#include "Components/ACFProceduralMotionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"

namespace ACFProceduralMotionDebug
{
	static TAutoConsoleVariable<int32> CVarCapture(
		TEXT("ACF.FirstPerson.Debug.Capture"),
		0,
		TEXT("Capture the procedural motion stack each evaluation so tooling can read it.\n")
		TEXT("Turned on automatically while the Motion Inspector is open.\n")
		TEXT("<=0: off, 1: on"),
		ECVF_Default);

	/** Only the pawn the local player is viewing, unless this is off. */
	static TAutoConsoleVariable<int32> CVarShowDebugAllMeshes(
		TEXT("ACF.FirstPerson.Debug.ShowAllMeshes"),
		0,
		TEXT("showdebug ACFFirstPerson draws every captured mesh rather than just the viewed pawn's.\n")
		TEXT("<=0: viewed pawn only, 1: all"),
		ECVF_Default);

	static TAutoConsoleVariable<float> CVarTextScale(
		TEXT("ACF.FirstPerson.Debug.TextScale"),
		0.9f,
		TEXT("Text scale for showdebug ACFFirstPerson. Lower fits more on a small screen."),
		ECVF_Default);

	static const FName DebugCategory(TEXT("ACFFirstPerson"));

	static FDelegateHandle ShowDebugHandle;

	/** Nominal column width before scaling. Wide enough for the longest op line. */
	static const float ColumnWidth = 460.0f;

	static const float BottomMargin = 24.0f;

	/**
	 * Draws top-to-bottom and wraps into a new column at the bottom of the screen.
	 *
	 * A stack with a weapon and a character easily runs past the bottom of a laptop screen, and anything
	 * that falls off is invisible with no indication it existed.
	 */
	struct FDebugTextCursor
	{
		UCanvas* Canvas = nullptr;
		float X = 0.0f;
		float Y = 0.0f;
		float TopY = 0.0f;
		float LineHeight = 12.0f;
		float Scale = 1.0f;
		float MaxY = 0.0f;

		void Draw(const FString& Text, const FLinearColor& Color, float Indent = 0.0f)
		{
			if (!Canvas)
			{
				return;
			}

			if (Y + LineHeight > Canvas->ClipY - BottomMargin)
			{
				X += ColumnWidth * Scale;
				Y = TopY;
			}

			FCanvasTextItem Item(FVector2D(X + Indent * Scale, Y), FText::FromString(Text), GEngine->GetSmallFont(), Color);
			Item.Scale = FVector2D(Scale, Scale);
			Canvas->DrawItem(Item);

			Y += LineHeight;
			MaxY = FMath::Max(MaxY, Y);
		}

		void Blank()
		{
			Y += LineHeight * 0.5f;
		}
	};

	static FString FormatVector(const FVector& Vector)
	{
		return FString::Printf(TEXT("%.1f,%.1f,%.1f"), Vector.X, Vector.Y, Vector.Z);
	}

	static FString FormatRotator(const FRotator& Rotator)
	{
		return FString::Printf(TEXT("%.1f,%.1f,%.1f"), Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
	}

	static const TCHAR* ToString(EACFProceduralMotionComposition Composition)
	{
		return Composition == EACFProceduralMotionComposition::Additive ? TEXT("Add") : TEXT("Lerp");
	}

	static const TCHAR* ToString(EACFProceduralMotionOutputSpace Space)
	{
		switch (Space)
		{
		case EACFProceduralMotionOutputSpace::Bone:		return TEXT("Bone");
		case EACFProceduralMotionOutputSpace::ParentBone:	return TEXT("Parent");
		default:										return TEXT("Comp");
		}
	}

	static void DrawSnapshot(FDebugTextCursor& Cursor, const FACFProceduralMotionDebugSnapshot& Snapshot)
	{
		Cursor.Draw(
			FString::Printf(
				TEXT("%s  bone=%s  aim=%.2f  %s%s"),
				*Snapshot.MeshName,
				*Snapshot.BoneName.ToString(),
				Snapshot.AimingState,
				Snapshot.bFirstPerson ? TEXT("FP") : TEXT("TP"),
				Snapshot.bHasReferencePose ? TEXT("  ref") : TEXT("")),
			FLinearColor::Yellow);

		int32 LastStage = INDEX_NONE;
		for (const FACFProceduralMotionDebugOp& Op : Snapshot.Ops)
		{
			if (Op.Stage != LastStage)
			{
				Cursor.Draw(FString::Printf(TEXT("Stage %d"), Op.Stage), FLinearColor(0.6f, 0.8f, 1.0f), 8.0f);
				LastStage = Op.Stage;
			}

			Cursor.Draw(
				FString::Printf(
					TEXT("%-22s %s/%-6s w%.2f  L(%s)  R(%s)"),
					*Op.ComponentName,
					ToString(Op.Composition),
					ToString(Op.Space),
					Op.Weight,
					*FormatVector(Op.Transform.GetLocation()),
					*FormatRotator(Op.Transform.Rotator())),
				FLinearColor::White,
				20.0f);
		}

		if (Snapshot.Ops.Num() == 0)
		{
			Cursor.Draw(TEXT("no ops emitted"), FLinearColor::Gray, 20.0f);
		}

		// State a component chose to publish about itself. Ops alone do not explain a stateful component.
		for (const FACFProceduralMotionDebugComponent& Entry : Snapshot.Components)
		{
			if (Entry.Values.Num() == 0)
			{
				continue;
			}

			const UACFProceduralMotionComponent* Component = Entry.Component.Get();
			Cursor.Draw(Component ? Component->GetName() : FString(TEXT("(gone)")), FLinearColor(0.6f, 0.8f, 1.0f), 8.0f);

			for (const FACFProceduralMotionDebugValue& Value : Entry.Values)
			{
				Cursor.Draw(FString::Printf(TEXT("%-22s %s"), *Value.Name.ToString(), *Value.Value), FLinearColor::White, 20.0f);
			}
		}
	}

	static void OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
	{
		if (!HUD || !Canvas || !DisplayInfo.IsDisplayOn(DebugCategory))
		{
			return;
		}

		// Turn capture on for as long as the overlay is up, so it costs nothing when it is not.
		CVarCapture->Set(1, ECVF_SetByConsole);

		TArray<FACFProceduralMotionDebugSnapshot> Snapshots;
		FACFProceduralMotionDebugRegistry::Get().GetSnapshots(Snapshots);

		const AActor* ViewTarget = HUD->PlayerOwner ? HUD->PlayerOwner->GetViewTarget() : nullptr;
		const bool bAllMeshes = CVarShowDebugAllMeshes.GetValueOnGameThread() > 0;

		const UFont* Font = GEngine->GetSmallFont();

		FDebugTextCursor Cursor;
		Cursor.Canvas = Canvas;
		Cursor.Scale = FMath::Max(CVarTextScale.GetValueOnGameThread(), 0.25f);
		Cursor.LineHeight = (Font ? Font->GetMaxCharHeight() : 12.0f) * Cursor.Scale;
		Cursor.X = 4.0f;
		Cursor.Y = YPos;
		Cursor.TopY = YPos;
		Cursor.MaxY = YPos;

		for (const FACFProceduralMotionDebugSnapshot& Snapshot : Snapshots)
		{
			const USkeletalMeshComponent* Mesh = Snapshot.Mesh.Get();
			if (!bAllMeshes && (!Mesh || Mesh->GetOwner() != ViewTarget))
			{
				continue;
			}

			DrawSnapshot(Cursor, Snapshot);
			Cursor.Blank();
		}

		// Hand the next showdebug category a sane cursor rather than wherever the last column ended.
		YPos = Cursor.MaxY;
	}
}

FACFProceduralMotionDebugRegistry& FACFProceduralMotionDebugRegistry::Get()
{
	static FACFProceduralMotionDebugRegistry Instance;
	return Instance;
}

bool FACFProceduralMotionDebugRegistry::IsCaptureEnabled()
{
	return ACFProceduralMotionDebug::CVarCapture.GetValueOnAnyThread() > 0;
}

void FACFProceduralMotionDebugRegistry::Register()
{
	if (!ACFProceduralMotionDebug::ShowDebugHandle.IsValid())
	{
		ACFProceduralMotionDebug::ShowDebugHandle = AHUD::OnShowDebugInfo.AddStatic(&ACFProceduralMotionDebug::OnShowDebugInfo);
	}
}

void FACFProceduralMotionDebugRegistry::Unregister()
{
	if (ACFProceduralMotionDebug::ShowDebugHandle.IsValid())
	{
		AHUD::OnShowDebugInfo.Remove(ACFProceduralMotionDebug::ShowDebugHandle);
		ACFProceduralMotionDebug::ShowDebugHandle.Reset();
	}
}

void FACFProceduralMotionDebugRegistry::Publish(FACFProceduralMotionDebugSnapshot&& Snapshot)
{
	FScopeLock Lock(&Mutex);

	const int32 Index = Snapshots.IndexOfByPredicate([&Snapshot](const FACFProceduralMotionDebugSnapshot& Existing)
	{
		return Existing.Mesh == Snapshot.Mesh;
	});

	if (Index != INDEX_NONE)
	{
		Snapshots[Index] = MoveTemp(Snapshot);
		return;
	}

	Snapshots.Add(MoveTemp(Snapshot));
}

void FACFProceduralMotionDebugRegistry::GetSnapshots(TArray<FACFProceduralMotionDebugSnapshot>& OutSnapshots) const
{
	const double Now = FPlatformTime::Seconds();

	FScopeLock Lock(&Mutex);

	for (const FACFProceduralMotionDebugSnapshot& Snapshot : Snapshots)
	{
		if (Now - Snapshot.TimeSeconds <= StaleSeconds && Snapshot.Mesh.IsValid())
		{
			OutSnapshots.Add(Snapshot);
		}
	}
}

bool FACFProceduralMotionDebugRegistry::GetSnapshotForMesh(const USkeletalMeshComponent* Mesh, FACFProceduralMotionDebugSnapshot& OutSnapshot) const
{
	if (!Mesh)
	{
		return false;
	}

	FScopeLock Lock(&Mutex);

	const FACFProceduralMotionDebugSnapshot* Found = Snapshots.FindByPredicate([Mesh](const FACFProceduralMotionDebugSnapshot& Snapshot)
	{
		return Snapshot.Mesh.Get() == Mesh;
	});

	if (!Found)
	{
		return false;
	}

	OutSnapshot = *Found;
	return true;
}

#endif
