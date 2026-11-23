// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UObject;
class UWorld;

namespace ACFFreeClimbNetwork
{
	bool IsSequenceNewer(uint16 A, uint16 B);
	double GetServerTimeSeconds(const UWorld* World);
	FQuat BuildAnchorFrameRotation(const FVector& AnchorNormal);
	FVector MakeAnchorRelativeLocation(const FVector& WorldLocation, const FVector& AnchorPoint, const FVector& AnchorNormal);
	FVector MakeWorldLocationFromAnchor(const FVector& RelativeLocation, const FVector& AnchorPoint, const FVector& AnchorNormal);
	
	template <typename TPayload>
	class TRemoteSnapshotBuffer
	{
	private:
		struct FEntry
		{
			uint16 Sequence = 0;
			float ServerTimeSeconds = 0.0f;
			TPayload Payload;
		};
		
	public:
		void Reset()
		{
			Entries.Reset();
			SessionId = 0;
			StreamObject.Reset();
			InterpolationTime = 0.0;
		}
		
		bool Add(uint16 InSessionId, uint16 InSequence, float InServerTimeSeconds, UObject* InStreamObject, float InterpolationDelay, TPayload Payload)
		{
			const bool bNewStream = Entries.IsEmpty() || SessionId != InSessionId || StreamObject.Get() != InStreamObject;
			if (bNewStream)
			{
				Reset();
				SessionId = InSessionId;
				StreamObject = InStreamObject;
				InterpolationTime = static_cast<double>(InServerTimeSeconds) - InterpolationDelay;
			}
			else if (!IsSequenceNewer(InSequence, Entries.Last().Sequence)) return false;
			
			FEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Sequence = InSequence;
			Entry.ServerTimeSeconds = InServerTimeSeconds;
			Entry.Payload = MoveTemp(Payload);
			if (InterpolationDelay <= 0.0f) { InterpolationTime = InServerTimeSeconds; }
			
			constexpr int32 MaxBufferedSnapshots = 32;
			if (Entries.Num() > MaxBufferedSnapshots)
			{
				Entries.RemoveAt(0, Entries.Num() - MaxBufferedSnapshots, EAllowShrinking::No);
			}
			return true;
		}
		
		bool Evaluate(float DeltaTime, const TPayload*& OutFrom, const TPayload*& OutTo, float& OutAlpha)
		{
			OutFrom = nullptr;
			OutTo = nullptr;
			OutAlpha = 0.0f;
			
			if (Entries.IsEmpty()) return false;
			
			InterpolationTime = FMath::Min(InterpolationTime + static_cast<double>(DeltaTime), static_cast<double>(Entries.Last().ServerTimeSeconds));
			while (Entries.Num() >= 2 && Entries[1].ServerTimeSeconds <= InterpolationTime)
			{
				Entries.RemoveAt(0, 1, EAllowShrinking::No);
			}
			
			const FEntry& FromEntry = Entries[0];
			const FEntry& ToEntry = Entries.Num() >= 2 ? Entries[1] : FromEntry;
			const float Duration = ToEntry.ServerTimeSeconds - FromEntry.ServerTimeSeconds;
			
			if (Duration > KINDA_SMALL_NUMBER)
			{
				OutAlpha = FMath::Clamp(static_cast<float>(InterpolationTime - static_cast<double>(FromEntry.ServerTimeSeconds)) / Duration, 0.0f, 1.0f);
			}
			
			OutFrom = &FromEntry.Payload;
			OutTo = &ToEntry.Payload;
			
			return true;
		}
		
		bool IsEmpty() const { return Entries.IsEmpty(); }
		
	private:
		TArray<FEntry> Entries;
		uint16 SessionId = 0;
		TWeakObjectPtr<UObject> StreamObject;
		double InterpolationTime = 0.0;
	};
}