// Copyright James Joslin. All Rights Reserved.

#include "WeatherWind.h"

namespace WeatherWindPrivate
{
	constexpr float MinimumRouteSpeed = 1.0f;
	constexpr float MinimumSegmentDuration = KINDA_SMALL_NUMBER;

	FVector SafePlanarDirection(const FVector& Vector, const FVector& Fallback)
	{
		const FVector Planar(Vector.X, Vector.Y, 0.0);
		if (!Planar.IsNearlyZero())
		{
			return Planar.GetSafeNormal();
		}

		const FVector PlanarFallback(Fallback.X, Fallback.Y, 0.0);
		return PlanarFallback.IsNearlyZero() ? FVector::ForwardVector : PlanarFallback.GetSafeNormal();
	}
}

FWeatherWindSettings::FWeatherWindSettings()
	: FieldTexture(FSoftObjectPath(TEXT("/WeatherEnvironmentSystem/Materials/T_WeatherWindField.T_WeatherWindField")))
	, MaterialParameterCollection(FSoftObjectPath(TEXT("/WeatherEnvironmentSystem/Materials/MPC_WeatherEnvironment.MPC_WeatherEnvironment")))
{
}

bool FWeatherWindRouteSolver::Start(
	const FVector& CurrentLocation,
	const TArray<FWeatherWindRoutePoint>& RoutePoints,
	FWeatherWindRouteState& InOutState)
{
	InOutState = FWeatherWindRouteState();
	if (RoutePoints.IsEmpty())
	{
		return false;
	}

	InOutState.bIsRunning = true;
	InOutState.TargetPointIndex = 0;
	InOutState.TravelDirection = 1;
	PrepareSegment(CurrentLocation, RoutePoints, InOutState);
	return true;
}

void FWeatherWindRouteSolver::Stop(FWeatherWindRouteState& InOutState)
{
	InOutState.bIsRunning = false;
	InOutState.bIsPaused = false;
	InOutState.PauseRemainingSeconds = 0.0f;
}

FVector FWeatherWindRouteSolver::Advance(
	const float DeltaSeconds,
	const TArray<FWeatherWindRoutePoint>& RoutePoints,
	const EWeatherWindRouteBehavior Behavior,
	FWeatherWindRouteState& InOutState,
	const FVector& CurrentLocation)
{
	if (!InOutState.bIsRunning || DeltaSeconds <= 0.0f || RoutePoints.IsEmpty())
	{
		return CurrentLocation;
	}

	if (!RoutePoints.IsValidIndex(InOutState.TargetPointIndex))
	{
		if (!Start(CurrentLocation, RoutePoints, InOutState))
		{
			return CurrentLocation;
		}
	}

	FVector Result = CurrentLocation;
	float RemainingSeconds = DeltaSeconds;
	const int32 IterationLimit = FMath::Max(16, RoutePoints.Num() * 4);
	for (int32 Iteration = 0;
		Iteration < IterationLimit && RemainingSeconds > KINDA_SMALL_NUMBER && InOutState.bIsRunning;
		++Iteration)
	{
		if (InOutState.bIsPaused)
		{
			const float Consumed = FMath::Min(RemainingSeconds, InOutState.PauseRemainingSeconds);
			InOutState.PauseRemainingSeconds -= Consumed;
			RemainingSeconds -= Consumed;
			if (InOutState.PauseRemainingSeconds > KINDA_SMALL_NUMBER)
			{
				break;
			}

			InOutState.bIsPaused = false;
			InOutState.PauseRemainingSeconds = 0.0f;
			if (!SelectNextTarget(RoutePoints, Behavior, InOutState))
			{
				break;
			}
			PrepareSegment(Result, RoutePoints, InOutState);
			continue;
		}

		const FWeatherWindRoutePoint& Target = RoutePoints[InOutState.TargetPointIndex];
		const float ArrivalRadius = FMath::Max(Target.ArrivalRadius, 0.0f);
		const bool bAlreadyArrived = FVector::DistSquared(Result, Target.Location)
			<= FMath::Square(ArrivalRadius);
		if (bAlreadyArrived
			|| InOutState.SegmentDurationSeconds <= WeatherWindPrivate::MinimumSegmentDuration)
		{
			Result = Target.Location;
			if (Target.PauseDuration > KINDA_SMALL_NUMBER)
			{
				InOutState.bIsPaused = true;
				InOutState.PauseRemainingSeconds = Target.PauseDuration;
			}
			else if (SelectNextTarget(RoutePoints, Behavior, InOutState))
			{
				PrepareSegment(Result, RoutePoints, InOutState);
			}
			continue;
		}

		const float SegmentRemaining = FMath::Max(
			InOutState.SegmentDurationSeconds - InOutState.SegmentElapsedSeconds,
			0.0f);
		const float Consumed = FMath::Min(RemainingSeconds, SegmentRemaining);
		InOutState.SegmentElapsedSeconds += Consumed;
		RemainingSeconds -= Consumed;

		const float LinearAlpha = FMath::Clamp(
			InOutState.SegmentElapsedSeconds / InOutState.SegmentDurationSeconds,
			0.0f,
			1.0f);
		Result = FMath::Lerp(
			InOutState.SegmentStart,
			Target.Location,
			ApplyEasing(LinearAlpha, Target.Easing));

		if (LinearAlpha >= 1.0f - KINDA_SMALL_NUMBER
			|| FVector::DistSquared(Result, Target.Location) <= FMath::Square(ArrivalRadius))
		{
			Result = Target.Location;
			if (Target.PauseDuration > KINDA_SMALL_NUMBER)
			{
				InOutState.bIsPaused = true;
				InOutState.PauseRemainingSeconds = Target.PauseDuration;
			}
			else if (SelectNextTarget(RoutePoints, Behavior, InOutState))
			{
				PrepareSegment(Result, RoutePoints, InOutState);
			}
		}
	}

	return Result;
}

float FWeatherWindRouteSolver::ApplyEasing(
	const float Alpha,
	const EWeatherWindRouteEasing Easing)
{
	const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);
	switch (Easing)
	{
	case EWeatherWindRouteEasing::EaseIn:
		return Clamped * Clamped;
	case EWeatherWindRouteEasing::EaseOut:
		return 1.0f - FMath::Square(1.0f - Clamped);
	case EWeatherWindRouteEasing::EaseInOut:
		return Clamped * Clamped * (3.0f - 2.0f * Clamped);
	default:
		return Clamped;
	}
}

void FWeatherWindRouteSolver::PrepareSegment(
	const FVector& CurrentLocation,
	const TArray<FWeatherWindRoutePoint>& RoutePoints,
	FWeatherWindRouteState& InOutState)
{
	if (!RoutePoints.IsValidIndex(InOutState.TargetPointIndex))
	{
		Stop(InOutState);
		return;
	}

	const FWeatherWindRoutePoint& Target = RoutePoints[InOutState.TargetPointIndex];
	InOutState.SegmentStart = CurrentLocation;
	InOutState.SegmentElapsedSeconds = 0.0f;
	InOutState.SegmentDurationSeconds = FVector::Distance(CurrentLocation, Target.Location)
		/ FMath::Max(Target.TravelSpeed, WeatherWindPrivate::MinimumRouteSpeed);
}

bool FWeatherWindRouteSolver::SelectNextTarget(
	const TArray<FWeatherWindRoutePoint>& RoutePoints,
	const EWeatherWindRouteBehavior Behavior,
	FWeatherWindRouteState& InOutState)
{
	if (RoutePoints.Num() <= 1)
	{
		if (Behavior == EWeatherWindRouteBehavior::Once)
		{
			Stop(InOutState);
			return false;
		}
		InOutState.TargetPointIndex = 0;
		return true;
	}

	const int32 Candidate = InOutState.TargetPointIndex + InOutState.TravelDirection;
	if (RoutePoints.IsValidIndex(Candidate))
	{
		InOutState.TargetPointIndex = Candidate;
		return true;
	}

	switch (Behavior)
	{
	case EWeatherWindRouteBehavior::Loop:
		InOutState.TravelDirection = 1;
		InOutState.TargetPointIndex = 0;
		return true;
	case EWeatherWindRouteBehavior::PingPong:
		InOutState.TravelDirection *= -1;
		InOutState.TargetPointIndex += InOutState.TravelDirection;
		return true;
	default:
		Stop(InOutState);
		return false;
	}
}

FVector FWeatherWindMath::ResolveBaseDirection(
	const FVector& CellCenter,
	const FVector& DirectorLocation,
	const FVector& DirectorForward,
	const FVector& PreviousWind,
	const FWeatherWindSettings& Settings)
{
	const FVector ToDirector(
		DirectorLocation.X - CellCenter.X,
		DirectorLocation.Y - CellCenter.Y,
		0.0);
	if (ToDirector.SizeSquared2D() > FMath::Square(FMath::Max(Settings.DirectorDeadZoneRadius, 0.0f)))
	{
		return ToDirector.GetSafeNormal();
	}

	const FVector PreviousDirection(PreviousWind.X, PreviousWind.Y, 0.0);
	if (!PreviousDirection.IsNearlyZero())
	{
		return PreviousDirection.GetSafeNormal();
	}

	return WeatherWindPrivate::SafePlanarDirection(DirectorForward, Settings.DefaultDirection);
}

FVector FWeatherWindMath::CalculateTargetWind(
	const FVector& CellCenter,
	const FVector& DirectorLocation,
	const FVector& DirectorForward,
	const FVector& PreviousWind,
	const double SimulationTimeSeconds,
	const FWeatherWindSettings& Settings,
	float& OutGust)
{
	FVector Direction = ResolveBaseDirection(
		CellCenter,
		DirectorLocation,
		DirectorForward,
		PreviousWind,
		Settings);

	const float CurlStrength = FMath::Clamp(Settings.CurlNoiseStrength, 0.0f, 1.0f);
	if (CurlStrength > 0.0f)
	{
		const float Scale = FMath::Max(Settings.CurlNoiseWorldScale, 1.0f);
		const float TimeOffset = static_cast<float>(SimulationTimeSeconds) * Settings.CurlNoiseScrollSpeed;
		const FVector2D NoiseCoordinate(
			static_cast<float>(CellCenter.X / Scale) + TimeOffset,
			static_cast<float>(CellCenter.Y / Scale) - TimeOffset);
		const float Angle = FMath::PerlinNoise2D(NoiseCoordinate) * UE_PI;
		const FVector CurlDirection(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		Direction = WeatherWindPrivate::SafePlanarDirection(
			FMath::Lerp(Direction, CurlDirection, CurlStrength),
			Direction);
	}

	const float GustScale = FMath::Max(Settings.GustWorldScale, 1.0f);
	const float GustTime = static_cast<float>(SimulationTimeSeconds)
		* FMath::Max(Settings.GustFrequency, 0.0f);
	const FVector2D GustCoordinate(
		static_cast<float>(CellCenter.X / GustScale) + GustTime,
		static_cast<float>(CellCenter.Y / GustScale) + GustTime * 0.6180339f);
	const float NormalizedNoise = FMath::Clamp(
		FMath::PerlinNoise2D(GustCoordinate) * 0.5f + 0.5f,
		0.0f,
		1.0f);
	OutGust = NormalizedNoise * FMath::Clamp(Settings.GustStrength, 0.0f, 1.0f);
	const float Speed = FMath::Clamp(
		Settings.BaseWindSpeed * (1.0f + OutGust * FMath::Max(Settings.GustSpeedMultiplier, 0.0f)),
		0.0f,
		FMath::Max(Settings.MaximumWindSpeed, 1.0f));
	return Direction * Speed;
}

FVector FWeatherWindMath::SmoothWind(
	const FVector& PreviousWind,
	const FVector& TargetWind,
	const float DeltaSeconds,
	const float SmoothingRate)
{
	if (DeltaSeconds <= 0.0f || SmoothingRate <= 0.0f || PreviousWind.ContainsNaN())
	{
		return TargetWind;
	}

	const float Alpha = 1.0f - FMath::Exp(-SmoothingRate * DeltaSeconds);
	return FMath::Lerp(PreviousWind, TargetWind, FMath::Clamp(Alpha, 0.0f, 1.0f));
}

FVector2D FWeatherWindMath::WorldToFieldUV(
	const FVector& WorldLocation,
	const FWeatherGridInfo& GridInfo)
{
	const FVector Size = GridInfo.GridBounds.GetSize();
	if (!GridInfo.bIsValid || Size.X <= SMALL_NUMBER || Size.Y <= SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}

	return FVector2D(
		FMath::Clamp((WorldLocation.X - GridInfo.GridBounds.Min.X) / Size.X, 0.0, 1.0),
		FMath::Clamp((WorldLocation.Y - GridInfo.GridBounds.Min.Y) / Size.Y, 0.0, 1.0));
}

FColor FWeatherWindMath::EncodeFieldTexel(
	const FVector& WindVector,
	const float Gust,
	const float MaximumWindSpeed)
{
	const FVector Direction = WeatherWindPrivate::SafePlanarDirection(WindVector, FVector::ForwardVector);
	return FColor(
		static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Direction.X * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f)),
		static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Direction.Y * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f)),
		static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(WindVector.Size2D() / FMath::Max(MaximumWindSpeed, 1.0f), 0.0f, 1.0f) * 255.0f)),
		static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Gust, 0.0f, 1.0f) * 255.0f)));
}

void FWeatherWindMath::DecodeFieldTexel(
	const FColor& Texel,
	const float MaximumWindSpeed,
	FVector& OutWindVector,
	float& OutGust)
{
	const FVector EncodedDirection(
		static_cast<float>(Texel.R) / 255.0f * 2.0f - 1.0f,
		static_cast<float>(Texel.G) / 255.0f * 2.0f - 1.0f,
		0.0f);
	const FVector Direction = WeatherWindPrivate::SafePlanarDirection(EncodedDirection, FVector::ForwardVector);
	const float Speed = static_cast<float>(Texel.B) / 255.0f * FMath::Max(MaximumWindSpeed, 1.0f);
	OutWindVector = Direction * Speed;
	OutGust = static_cast<float>(Texel.A) / 255.0f;
}

FWeatherFoliageMaterialState FWeatherWindMath::BuildFoliageMaterialState(
	const FVector& WindDirection,
	const float WindSpeed,
	const float Gust,
	const FWeatherFoliageMaterialSettings& Settings)
{
	FWeatherFoliageMaterialState Result;
	const FVector Direction = WeatherWindPrivate::SafePlanarDirection(
		WindDirection,
		FVector::ForwardVector);
	const float SpeedScale = FMath::Clamp(
		FMath::Max(WindSpeed, 0.0f) / FMath::Max(Settings.ReferenceWindSpeed, 1.0f),
		0.0f,
		4.0f);
	const float AnimationScale = FMath::Sqrt(SpeedScale)
		* (1.0f + FMath::Clamp(Gust, 0.0f, 1.0f)
			* FMath::Max(Settings.GustAnimationResponse, 0.0f));

	Result.GrassWindSmallSize = FMath::Max(Settings.GrassWindSmallSize, 1.0f);
	Result.GrassWindLargeSize = FMath::Max(Settings.GrassWindLargeSize, 1.0f);
	Result.GrassWindSmallAmplification = Settings.GrassWindSmallAmplification * SpeedScale;
	Result.GrassWindLargeAmplification = Settings.GrassWindLargeAmplification * SpeedScale;
	Result.SimpleWindIntensity = FMath::Max(Settings.SimpleWindIntensity, 0.0f) * SpeedScale;
	Result.SimpleWindSpeed = FMath::Max(Settings.SimpleWindSpeed, 0.0f) * AnimationScale;
	Result.WindSwayGradient = Settings.WindSwayGradient;
	Result.WindSwayGustFrequency = FMath::Max(Settings.WindSwayGustFrequency, 0.0f)
		* AnimationScale;
	Result.WindSwayIntensity = FMath::Max(Settings.WindSwayIntensity, 0.0f) * SpeedScale;
	Result.WindSwayOffset = Settings.WindSwayOffset;
	Result.WindSwayDirection = FLinearColor(
		static_cast<float>(-Direction.Y) * Settings.WindSwayAxisHorizontal,
		static_cast<float>(Direction.X) * Settings.WindSwayAxisHorizontal,
		Settings.WindSwayAxisVertical,
		0.0f);
	return Result;
}
