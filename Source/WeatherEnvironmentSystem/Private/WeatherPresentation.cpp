// Copyright James Joslin. All Rights Reserved.

#include "WeatherPresentation.h"

namespace
{
	uint32 BuildLightningSeed(
		const FWeatherLightningPresentationSettings& Settings,
		const int32 EnvironmentSeed,
		const FWeatherCellCoord& CellCoord,
		const int32 StrikeSequence,
		const uint32 StreamSalt)
	{
		uint32 Seed = HashCombine(::GetTypeHash(EnvironmentSeed), ::GetTypeHash(Settings.RandomSeedSalt));
		Seed = HashCombine(Seed, GetTypeHash(CellCoord));
		Seed = HashCombine(Seed, ::GetTypeHash(StrikeSequence));
		return HashCombine(Seed, StreamSalt);
	}
}

float FWeatherPresentationMath::AdvanceTransitionAlpha(
	const float CurrentAlpha,
	const bool bFadeIn,
	const float DeltaSeconds,
	const float FadeInSeconds,
	const float FadeOutSeconds)
{
	const float Target = bFadeIn ? 1.0f : 0.0f;
	const float Duration = bFadeIn ? FadeInSeconds : FadeOutSeconds;
	if (Duration <= UE_SMALL_NUMBER)
	{
		return Target;
	}

	return FMath::Clamp(
		FMath::FInterpConstantTo(
			FMath::Clamp(CurrentAlpha, 0.0f, 1.0f),
			Target,
			FMath::Max(DeltaSeconds, 0.0f),
			1.0f / Duration),
		0.0f,
		1.0f);
}

bool FWeatherPresentationMath::IsCellWithinPresentationRange(
	const FBox& CellBounds,
	const TConstArrayView<FVector> ViewLocations,
	const double Range,
	double& OutDistanceSquared)
{
	OutDistanceSquared = TNumericLimits<double>::Max();
	if (!CellBounds.IsValid || ViewLocations.IsEmpty())
	{
		return false;
	}

	for (const FVector& ViewLocation : ViewLocations)
	{
		const double DeltaX = FMath::Max3(
			CellBounds.Min.X - ViewLocation.X,
			0.0,
			ViewLocation.X - CellBounds.Max.X);
		const double DeltaY = FMath::Max3(
			CellBounds.Min.Y - ViewLocation.Y,
			0.0,
			ViewLocation.Y - CellBounds.Max.Y);
		OutDistanceSquared = FMath::Min(
			OutDistanceSquared,
			DeltaX * DeltaX + DeltaY * DeltaY);
	}

	const double ClampedRange = FMath::Max(Range, 0.0);
	return OutDistanceSquared <= ClampedRange * ClampedRange;
}

void FWeatherPresentationMath::SelectPrecipitationCells(
	const FWeatherGrid& Grid,
	const TConstArrayView<FVector> ViewLocations,
	const FWeatherPrecipitationPresentationSettings& Settings,
	const int32 MaximumResults,
	TArray<FWeatherPrecipitationCandidate>& OutCandidates)
{
	OutCandidates.Reset();
	const FWeatherGridInfo& Info = Grid.GetInfo();
	if (!Info.bIsValid || ViewLocations.IsEmpty() || MaximumResults <= 0)
	{
		return;
	}

	const double Range = FMath::Max(
		static_cast<double>(Settings.PresentationRadiusInCells) * Info.CellSize,
		0.0);
	TSet<FWeatherCellCoord> NearbyCells;
	for (const FVector& ViewLocation : ViewLocations)
	{
		TArray<FWeatherCellCoord> ViewCells;
		Grid.GetCellsIntersectingBounds(
			FBox(
				FVector(ViewLocation.X - Range, ViewLocation.Y - Range, Info.GridBounds.Min.Z),
				FVector(ViewLocation.X + Range, ViewLocation.Y + Range, Info.GridBounds.Max.Z)),
			ViewCells);
		NearbyCells.Append(ViewCells);
	}

	OutCandidates.Reserve(FMath::Min(NearbyCells.Num(), MaximumResults));
	for (const FWeatherCellCoord& Coord : NearbyCells)
	{
		const FWeatherCellState* State = Grid.FindCell(Coord);
		if (!State || !State->bIsRaining)
		{
			continue;
		}

		double DistanceSquared = 0.0;
		if (!IsCellWithinPresentationRange(
			Grid.GetCellBounds(Coord),
			ViewLocations,
			Range,
			DistanceSquared))
		{
			continue;
		}

		const float Proximity = Range <= UE_DOUBLE_SMALL_NUMBER
			? (DistanceSquared <= UE_DOUBLE_SMALL_NUMBER ? 1.0f : 0.0f)
			: 1.0f - FMath::Clamp(
				static_cast<float>(FMath::Sqrt(DistanceSquared) / Range),
				0.0f,
				1.0f);
		FWeatherPrecipitationCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
		Candidate.CellCoord = Coord;
		Candidate.State = *State;
		Candidate.DistanceSquared = DistanceSquared;
		Candidate.Priority = FMath::Max(Settings.IntensityPriorityWeight, 0.0f)
				* FMath::Clamp(State->RainIntensity, 0.0f, 1.0f)
			+ FMath::Max(Settings.ProximityPriorityWeight, 0.0f) * Proximity;
	}

	OutCandidates.Sort([](
		const FWeatherPrecipitationCandidate& A,
		const FWeatherPrecipitationCandidate& B)
	{
		if (!FMath::IsNearlyEqual(A.Priority, B.Priority))
		{
			return A.Priority > B.Priority;
		}
		if (!FMath::IsNearlyEqual(A.DistanceSquared, B.DistanceSquared))
		{
			return A.DistanceSquared < B.DistanceSquared;
		}
		if (A.CellCoord.Y != B.CellCoord.Y)
		{
			return A.CellCoord.Y < B.CellCoord.Y;
		}
		return A.CellCoord.X < B.CellCoord.X;
	});

	if (OutCandidates.Num() > MaximumResults)
	{
		OutCandidates.SetNum(MaximumResults, EAllowShrinking::No);
	}
}

float FWeatherPresentationMath::CalculateLightningStrikeInterval(
	const FWeatherLightningPresentationSettings& Settings,
	const int32 EnvironmentSeed,
	const FWeatherCellCoord& CellCoord,
	const int32 StrikeSequence,
	const float LightningPotential)
{
	const float Minimum = FMath::Max(
		FMath::Min(Settings.MinimumStrikeIntervalSeconds, Settings.MaximumStrikeIntervalSeconds),
		0.01f);
	const float Maximum = FMath::Max(
		FMath::Max(Settings.MinimumStrikeIntervalSeconds, Settings.MaximumStrikeIntervalSeconds),
		Minimum);
	FRandomStream Stream(static_cast<int32>(BuildLightningSeed(
		Settings,
		EnvironmentSeed,
		CellCoord,
		StrikeSequence,
		0xA341316Cu)));
	const float RandomInterval = Stream.FRandRange(Minimum, Maximum);
	const float EligibilityThreshold = FMath::Clamp(Settings.MinimumLightningPotential, 0.0f, 1.0f);
	const float PotentialAlpha = EligibilityThreshold >= 1.0f
		? 1.0f
		: FMath::Clamp(
			(LightningPotential - EligibilityThreshold) / (1.0f - EligibilityThreshold),
			0.0f,
			1.0f);
	const float PotentialMultiplier = FMath::Lerp(
		FMath::Max(Settings.LowPotentialIntervalMultiplier, 1.0f),
		1.0f,
		PotentialAlpha);
	return RandomInterval * PotentialMultiplier;
}

FVector2D FWeatherPresentationMath::CalculateLightningUnitOffset(
	const FWeatherLightningPresentationSettings& Settings,
	const int32 EnvironmentSeed,
	const FWeatherCellCoord& CellCoord,
	const int32 StrikeSequence)
{
	FRandomStream Stream(static_cast<int32>(BuildLightningSeed(
		Settings,
		EnvironmentSeed,
		CellCoord,
		StrikeSequence,
		0xC8013EA4u)));
	const float UniformX = Stream.FRand();
	const float UniformY = Stream.FRand();
	const float WeightedX = (Stream.FRand() + Stream.FRand()) * 0.5f;
	const float WeightedY = (Stream.FRand() + Stream.FRand()) * 0.5f;
	const float CenterWeight = FMath::Clamp(Settings.CenterWeight, 0.0f, 1.0f);
	const float Inset = FMath::Clamp(Settings.CellEdgeInsetFraction, 0.0f, 0.49f);
	return FVector2D(
		FMath::Lerp(Inset, 1.0f - Inset, FMath::Lerp(UniformX, WeightedX, CenterWeight)),
		FMath::Lerp(Inset, 1.0f - Inset, FMath::Lerp(UniformY, WeightedY, CenterWeight)));
}

bool FWeatherPresentationMath::ResolveGroundTraceResult(
	const bool bTraceEnabled,
	const bool bRequireHit,
	const bool bHit,
	const FVector& FallbackLocation,
	const FVector& HitLocation,
	FVector& OutLocation)
{
	if (bTraceEnabled && bHit)
	{
		OutLocation = HitLocation;
		return true;
	}

	OutLocation = FallbackLocation;
	return !bTraceEnabled || !bRequireHit;
}
