// Copyright James Joslin. All Rights Reserved.

#include "WeatherSimulation.h"

namespace WeatherSimulationPrivate
{
	bool MatchesBoolean(const bool bValue, const EWeatherBooleanRequirement Requirement)
	{
		switch (Requirement)
		{
		case EWeatherBooleanRequirement::RequiredFalse:
			return !bValue;
		case EWeatherBooleanRequirement::RequiredTrue:
			return bValue;
		default:
			return true;
		}
	}

	int32 PositiveModulo(const int32 Value, const int32 Modulus)
	{
		return Modulus > 0 ? (Value % Modulus + Modulus) % Modulus : 0;
	}

	float SampleRange(const float A, const float B, FRandomStream& RandomStream)
	{
		return RandomStream.FRandRange(FMath::Min(A, B), FMath::Max(A, B));
	}

	struct FCellAccumulator
	{
		double Weight = 0.0;
		double CloudCoverage = 0.0;
		double CloudDensity = 0.0;
		double Humidity = 0.0;
		double TemperatureCelsius = 0.0;
		double PressureHpa = 0.0;
		double Storminess = 0.0;
		double RainIntensity = 0.0;
		double LightningPotential = 0.0;

		void Add(const FWeatherSeedValues& Values, const double AddedWeight)
		{
			Weight += AddedWeight;
			CloudCoverage += Values.CloudCoverage * AddedWeight;
			CloudDensity += Values.CloudDensity * AddedWeight;
			Humidity += Values.Humidity * AddedWeight;
			TemperatureCelsius += Values.TemperatureCelsius * AddedWeight;
			PressureHpa += Values.PressureHpa * AddedWeight;
			Storminess += Values.Storminess * AddedWeight;
			RainIntensity += Values.RainIntensity * AddedWeight;
			LightningPotential += Values.LightningPotential * AddedWeight;
		}
	};
}

bool FWeatherFloatRange::Contains(const float Value, const float Expansion) const
{
	if (!bEnabled)
	{
		return true;
	}

	const float Low = FMath::Min(Minimum, Maximum) - FMath::Max(Expansion, 0.0f);
	const float High = FMath::Max(Minimum, Maximum) + FMath::Max(Expansion, 0.0f);
	return Value >= Low && Value <= High;
}

bool FWeatherTypeClassificationRule::Matches(
	const FWeatherCellState& State,
	const bool bApplyHysteresis) const
{
	const float Expansion = bApplyHysteresis ? FMath::Max(Hysteresis, 0.0f) : 0.0f;
	return CloudCoverage.Contains(State.CloudCoverage, Expansion)
		&& CloudDensity.Contains(State.CloudDensity, Expansion)
		&& Humidity.Contains(State.Humidity, Expansion)
		&& TemperatureCelsius.Contains(State.TemperatureCelsius, Expansion)
		&& PressureHpa.Contains(State.PressureHpa, Expansion)
		&& Storminess.Contains(State.Storminess, Expansion)
		&& RainIntensity.Contains(State.RainIntensity, Expansion)
		&& LightningPotential.Contains(State.LightningPotential, Expansion)
		&& WeatherSimulationPrivate::MatchesBoolean(State.bIsRaining, IsRaining)
		&& WeatherSimulationPrivate::MatchesBoolean(State.bIsStorm, IsStorm);
}

EWeatherType UWeatherTypeLookupDataAsset::ClassifyWeather(
	const FWeatherCellState& State,
	const EWeatherType CurrentType) const
{
	const FWeatherTypeClassificationRule* BestRule = nullptr;
	for (const FWeatherTypeClassificationRule& Rule : Rules)
	{
		if (!Rule.Matches(State, Rule.WeatherType == CurrentType))
		{
			continue;
		}

		// Strictly greater preserves asset order for equal-priority rules.
		if (!BestRule || Rule.Priority > BestRule->Priority)
		{
			BestRule = &Rule;
		}
	}

	return BestRule ? BestRule->WeatherType : EWeatherType::Clear;
}

const FWeatherTypeClassificationRule* UWeatherTypeLookupDataAsset::FindRule(
	const EWeatherType WeatherType) const
{
	for (const FWeatherTypeClassificationRule& Rule : Rules)
	{
		if (Rule.WeatherType == WeatherType)
		{
			return &Rule;
		}
	}
	return nullptr;
}

bool UWeatherTypeLookupDataAsset::GetPresentationProfile(
	const EWeatherType WeatherType,
	FWeatherTypePresentationProfile& OutPresentation) const
{
	const FWeatherTypeClassificationRule* Rule = FindRule(WeatherType);
	if (!Rule)
	{
		OutPresentation = FWeatherTypePresentationProfile();
		return false;
	}

	OutPresentation = Rule->Presentation;
	return true;
}

FWeatherFrontLifecycleSettings::FWeatherFrontLifecycleSettings()
{
	auto AddArchetype = [this](
		const EWeatherType Type,
		const float Weight,
		const FVector2D SigmaCells,
		const FVector2D Strength,
		const FVector2D LifetimeSeconds,
		const FVector2D MovementMultiplier)
	{
		FWeatherFrontArchetype& Archetype = Archetypes.AddDefaulted_GetRef();
		Archetype.WeatherType = Type;
		Archetype.SpawnWeight = Weight;
		Archetype.SigmaCellRange = SigmaCells;
		Archetype.StrengthRange = Strength;
		Archetype.LifetimeSecondsRange = LifetimeSeconds;
		Archetype.MovementMultiplierRange = MovementMultiplier;
	};

	// The baseline already supplies fair weather. These weights still allow a
	// clear front to open gaps while keeping severe weather deliberately rare.
	AddArchetype(
		EWeatherType::Clear,
		20.0f,
		FVector2D(0.8, 1.5),
		FVector2D(0.5, 1.0),
		FVector2D(900.0, 1800.0),
		FVector2D(0.7, 1.0));
	AddArchetype(
		EWeatherType::PartlyCloudy,
		30.0f,
		FVector2D(0.8, 1.5),
		FVector2D(0.6, 1.1),
		FVector2D(1200.0, 2400.0),
		FVector2D(0.7, 1.1));
	AddArchetype(
		EWeatherType::Overcast,
		25.0f,
		FVector2D(0.7, 1.3),
		FVector2D(0.7, 1.2),
		FVector2D(900.0, 2100.0),
		FVector2D(0.75, 1.15));
	AddArchetype(
		EWeatherType::Rain,
		15.0f,
		FVector2D(0.55, 1.0),
		FVector2D(0.9, 1.4),
		FVector2D(600.0, 1500.0),
		FVector2D(0.85, 1.2));
	AddArchetype(
		EWeatherType::HeavyRain,
		7.0f,
		FVector2D(0.4, 0.75),
		FVector2D(1.1, 1.7),
		FVector2D(420.0, 900.0),
		FVector2D(0.9, 1.25));
	AddArchetype(
		EWeatherType::Storm,
		3.0f,
		FVector2D(0.3, 0.6),
		FVector2D(1.4, 2.1),
		FVector2D(300.0, 720.0),
		FVector2D(1.0, 1.35));
}

FWeatherSimulationSettings::FWeatherSimulationSettings()
{
	BaselineValues.CloudCoverage = 0.05f;
	BaselineValues.CloudDensity = 0.05f;
	BaselineValues.Humidity = 0.25f;
	BaselineValues.TemperatureCelsius = 20.0f;
	BaselineValues.PressureHpa = 1013.25f;

	GeneratedValueRange.Minimum.CloudCoverage = 0.0f;
	GeneratedValueRange.Minimum.CloudDensity = 0.0f;
	GeneratedValueRange.Minimum.Humidity = 0.15f;
	GeneratedValueRange.Minimum.TemperatureCelsius = 8.0f;
	GeneratedValueRange.Minimum.PressureHpa = 985.0f;
	GeneratedValueRange.Minimum.Storminess = 0.0f;
	GeneratedValueRange.Minimum.RainIntensity = 0.0f;
	GeneratedValueRange.Minimum.LightningPotential = 0.0f;

	GeneratedValueRange.Maximum.CloudCoverage = 1.0f;
	GeneratedValueRange.Maximum.CloudDensity = 1.0f;
	GeneratedValueRange.Maximum.Humidity = 1.0f;
	GeneratedValueRange.Maximum.TemperatureCelsius = 30.0f;
	GeneratedValueRange.Maximum.PressureHpa = 1035.0f;
	GeneratedValueRange.Maximum.Storminess = 1.0f;
	GeneratedValueRange.Maximum.RainIntensity = 1.0f;
	GeneratedValueRange.Maximum.LightningPotential = 1.0f;

	auto AddPreset = [this](
		const EWeatherType Type,
		const float Cloud,
		const float Density,
		const float Humidity,
		const float Storm,
		const float Rain,
		const float Lightning)
	{
		FWeatherTypePreset& Preset = WeatherTypePresets.AddDefaulted_GetRef();
		Preset.WeatherType = Type;
		Preset.Values.CloudCoverage = Cloud;
		Preset.Values.CloudDensity = Density;
		Preset.Values.Humidity = Humidity;
		Preset.Values.Storminess = Storm;
		Preset.Values.RainIntensity = Rain;
		Preset.Values.LightningPotential = Lightning;
	};

	AddPreset(EWeatherType::Clear, 0.05f, 0.05f, 0.25f, 0.0f, 0.0f, 0.0f);
	AddPreset(EWeatherType::PartlyCloudy, 0.45f, 0.35f, 0.45f, 0.05f, 0.0f, 0.0f);
	AddPreset(EWeatherType::Overcast, 0.85f, 0.75f, 0.7f, 0.2f, 0.15f, 0.05f);
	AddPreset(EWeatherType::Rain, 0.9f, 0.85f, 0.85f, 0.4f, 0.7f, 0.15f);
	AddPreset(EWeatherType::HeavyRain, 1.0f, 0.95f, 0.95f, 0.6f, 0.9f, 0.45f);
	AddPreset(EWeatherType::Storm, 1.0f, 1.0f, 1.0f, 0.95f, 1.0f, 0.95f);
}

FGuid FWeatherSimulationMath::MakeDeterministicSeedId(
	const int32 EnvironmentSeed,
	const int32 SeedSerial)
{
	const uint32 Environment = static_cast<uint32>(EnvironmentSeed);
	const uint32 Serial = static_cast<uint32>(SeedSerial);
	return FGuid(
		Environment,
		Serial,
		HashCombineFast(Environment, Serial),
		HashCombineFast(Serial ^ 0x9e3779b9u, Environment ^ 0x85ebca6bu));
}

double FWeatherSimulationMath::GaussianWeight(
	const double Distance,
	const double Sigma,
	const double Strength)
{
	if (Sigma <= UE_DOUBLE_SMALL_NUMBER || Strength <= 0.0)
	{
		return 0.0;
	}

	const double NormalizedDistance = Distance / Sigma;
	return Strength * FMath::Exp(-0.5 * NormalizedDistance * NormalizedDistance);
}

bool FWeatherSimulationMath::ApplyBoundaryPolicy(
	FVector2D& InOutPosition,
	const FWeatherGridInfo& GridInfo,
	const EWeatherSeedBoundaryPolicy BoundaryPolicy)
{
	if (!GridInfo.bIsValid)
	{
		return false;
	}

	const FVector Minimum3D = GridInfo.GridBounds.Min;
	const FVector Maximum3D = GridInfo.GridBounds.Max;
	const FVector2D Minimum(Minimum3D.X, Minimum3D.Y);
	const FVector2D Maximum(Maximum3D.X, Maximum3D.Y);
	const FVector2D Size = Maximum - Minimum;
	if (Size.X <= UE_DOUBLE_SMALL_NUMBER || Size.Y <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}

	const bool bInside = InOutPosition.X >= Minimum.X && InOutPosition.X <= Maximum.X
		&& InOutPosition.Y >= Minimum.Y && InOutPosition.Y <= Maximum.Y;
	if (bInside)
	{
		return true;
	}

	switch (BoundaryPolicy)
	{
	case EWeatherSeedBoundaryPolicy::Clamp:
		InOutPosition.X = FMath::Clamp(InOutPosition.X, Minimum.X, Maximum.X);
		InOutPosition.Y = FMath::Clamp(InOutPosition.Y, Minimum.Y, Maximum.Y);
		return true;
	case EWeatherSeedBoundaryPolicy::Wrap:
		InOutPosition.X = Minimum.X + FMath::Fmod(FMath::Fmod(InOutPosition.X - Minimum.X, Size.X) + Size.X, Size.X);
		InOutPosition.Y = Minimum.Y + FMath::Fmod(FMath::Fmod(InOutPosition.Y - Minimum.Y, Size.Y) + Size.Y, Size.Y);
		return true;
	default:
		return false;
	}
}

void FWeatherSimulationMath::AdvectSeeds(
	TArray<FWeatherSeed>& InOutSeeds,
	const FWeatherGrid& Grid,
	const FWeatherSimulationSettings& Settings,
	const float StepSeconds)
{
	if (StepSeconds <= 0.0f || !Grid.GetInfo().bIsValid)
	{
		return;
	}

	for (int32 SeedIndex = InOutSeeds.Num() - 1; SeedIndex >= 0; --SeedIndex)
	{
		FWeatherSeed& Seed = InOutSeeds[SeedIndex];
		Seed.AgeSeconds += StepSeconds;
		if (Seed.LifetimeSeconds > 0.0f && Seed.AgeSeconds >= Seed.LifetimeSeconds)
		{
			InOutSeeds.RemoveAt(SeedIndex);
			continue;
		}

		const FVector SampleLocation(
			Seed.Position.X,
			Seed.Position.Y,
			Grid.GetInfo().GridBounds.GetCenter().Z);
		const FWeatherSample WindSample = Grid.GetWeatherAtLocationBilinear(SampleLocation);
		if (WindSample.bIsValid)
		{
			Seed.Position += FVector2D(WindSample.State.WindVector.X, WindSample.State.WindVector.Y)
				* static_cast<double>(StepSeconds * FMath::Max(Seed.MovementMultiplier, 0.0f));
		}

		if (!ApplyBoundaryPolicy(Seed.Position, Grid.GetInfo(), Settings.BoundaryPolicy))
		{
			InOutSeeds.RemoveAt(SeedIndex);
		}
	}
}

void FWeatherSimulationMath::RebuildCellFields(
	FWeatherGrid& Grid,
	const TArray<FWeatherSeed>& Seeds,
	const FWeatherSimulationSettings& Settings,
	const float StepSeconds,
	TArray<float>& InOutWeatherTypeDurations,
	int64* OutEvaluatedCellCount)
{
	if (OutEvaluatedCellCount)
	{
		*OutEvaluatedCellCount = 0;
	}
	const FWeatherGridInfo& Info = Grid.GetInfo();
	TArray<FWeatherCellState>& Cells = Grid.GetMutableCells();
	if (!Info.bIsValid || Cells.IsEmpty())
	{
		InOutWeatherTypeDurations.Reset();
		return;
	}

	const double BaselineWeight = FMath::Max(static_cast<double>(Settings.BaselineWeight), 0.0001);
	TArray<WeatherSimulationPrivate::FCellAccumulator> Accumulators;
	Accumulators.SetNum(Cells.Num());
	for (WeatherSimulationPrivate::FCellAccumulator& Accumulator : Accumulators)
	{
		Accumulator.Add(Settings.BaselineValues, BaselineWeight);
	}

	const double GridWidth = Info.Dimensions.X * Info.CellSize;
	const double GridHeight = Info.Dimensions.Y * Info.CellSize;
	for (const FWeatherSeed& Seed : Seeds)
	{
		const double Sigma = FMath::Max(Seed.Sigma, 1.0);
		const double InfluenceRadius = 3.0 * Sigma;
		const int32 RawMinimumX = FMath::FloorToInt((Seed.Position.X - InfluenceRadius - Info.Origin.X) / Info.CellSize);
		const int32 RawMaximumX = FMath::FloorToInt((Seed.Position.X + InfluenceRadius - Info.Origin.X) / Info.CellSize);
		const int32 RawMinimumY = FMath::FloorToInt((Seed.Position.Y - InfluenceRadius - Info.Origin.Y) / Info.CellSize);
		const int32 RawMaximumY = FMath::FloorToInt((Seed.Position.Y + InfluenceRadius - Info.Origin.Y) / Info.CellSize);
		const bool bWrap = Settings.BoundaryPolicy == EWeatherSeedBoundaryPolicy::Wrap;
		auto BuildAxisCandidates = [bWrap](
			const int32 RawMinimum,
			const int32 RawMaximum,
			const int32 Dimension)
		{
			TArray<int32> Candidates;
			if (!bWrap)
			{
				const int32 Minimum = FMath::Clamp(RawMinimum, 0, Dimension - 1);
				const int32 Maximum = FMath::Clamp(RawMaximum, 0, Dimension - 1);
				for (int32 Value = Minimum; Value <= Maximum; ++Value)
				{
					Candidates.Add(Value);
				}
				return Candidates;
			}

			const int64 RawCount = static_cast<int64>(RawMaximum) - RawMinimum + 1;
			if (RawCount >= Dimension)
			{
				for (int32 Value = 0; Value < Dimension; ++Value)
				{
					Candidates.Add(Value);
				}
				return Candidates;
			}

			Candidates.Reserve(static_cast<int32>(FMath::Max<int64>(RawCount, 0)));
			for (int32 Value = RawMinimum; Value <= RawMaximum; ++Value)
			{
				Candidates.Add(WeatherSimulationPrivate::PositiveModulo(Value, Dimension));
			}
			return Candidates;
		};

		const TArray<int32> CandidateXs = BuildAxisCandidates(
			RawMinimumX,
			RawMaximumX,
			Info.Dimensions.X);
		const TArray<int32> CandidateYs = BuildAxisCandidates(
			RawMinimumY,
			RawMaximumY,
			Info.Dimensions.Y);
		for (const int32 Y : CandidateYs)
		{
			for (const int32 X : CandidateXs)
			{
				const int32 CellIndex = Y * Info.Dimensions.X + X;
				if (OutEvaluatedCellCount)
				{
					++(*OutEvaluatedCellCount);
				}

				const FVector& Center = Cells[CellIndex].WorldCenter;
				double DeltaX = FMath::Abs(Center.X - Seed.Position.X);
				double DeltaY = FMath::Abs(Center.Y - Seed.Position.Y);
				if (bWrap)
				{
					DeltaX = FMath::Min(DeltaX, GridWidth - DeltaX);
					DeltaY = FMath::Min(DeltaY, GridHeight - DeltaY);
				}
				const double Distance = FMath::Sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
				if (Distance > InfluenceRadius)
				{
					continue;
				}

				const double Weight = GaussianWeight(Distance, Sigma, Seed.Strength);
				if (Weight > UE_DOUBLE_SMALL_NUMBER)
				{
					Accumulators[CellIndex].Add(Seed.Values, Weight);
				}
			}
		}
	}

	if (InOutWeatherTypeDurations.Num() != Cells.Num())
	{
		InOutWeatherTypeDurations.Init(0.0f, Cells.Num());
	}

	for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
	{
		FWeatherCellState& State = Cells[CellIndex];
		const WeatherSimulationPrivate::FCellAccumulator& Accumulator = Accumulators[CellIndex];
		const double InverseWeight = 1.0 / FMath::Max(Accumulator.Weight, UE_DOUBLE_SMALL_NUMBER);
		State.CloudCoverage = FMath::Clamp(static_cast<float>(Accumulator.CloudCoverage * InverseWeight), 0.0f, 1.0f);
		State.CloudDensity = FMath::Clamp(static_cast<float>(Accumulator.CloudDensity * InverseWeight), 0.0f, 1.0f);
		State.Humidity = FMath::Clamp(static_cast<float>(Accumulator.Humidity * InverseWeight), 0.0f, 1.0f);
		State.TemperatureCelsius = static_cast<float>(Accumulator.TemperatureCelsius * InverseWeight);
		State.PressureHpa = static_cast<float>(Accumulator.PressureHpa * InverseWeight);
		State.Storminess = FMath::Clamp(static_cast<float>(Accumulator.Storminess * InverseWeight), 0.0f, 1.0f);
		State.RainIntensity = FMath::Clamp(static_cast<float>(Accumulator.RainIntensity * InverseWeight), 0.0f, 1.0f);
		State.LightningPotential = FMath::Clamp(static_cast<float>(Accumulator.LightningPotential * InverseWeight), 0.0f, 1.0f);

		State.bIsRaining = State.bIsRaining
			? State.RainIntensity >= Settings.RainExitThreshold
			: State.RainIntensity >= Settings.RainEnterThreshold;
		State.bIsStorm = State.bIsStorm
			? State.Storminess >= Settings.StormExitThreshold
			: State.Storminess >= Settings.StormEnterThreshold;

		const EWeatherType PreviousType = State.WeatherType;
		const EWeatherType CandidateType = Settings.WeatherTypeLookup
			? Settings.WeatherTypeLookup->ClassifyWeather(State, PreviousType)
			: ClassifyBuiltIn(State, PreviousType);
		if (CandidateType != PreviousType
			&& InOutWeatherTypeDurations[CellIndex] >= Settings.MinimumWeatherTypeDurationSeconds)
		{
			State.WeatherType = CandidateType;
			InOutWeatherTypeDurations[CellIndex] = 0.0f;
		}
		else
		{
			InOutWeatherTypeDurations[CellIndex] += FMath::Max(StepSeconds, 0.0f);
		}
	}
}

FWeatherSeedValues FWeatherSimulationMath::SampleValues(
	const FWeatherSeedValueRange& Range,
	FRandomStream& RandomStream)
{
	FWeatherSeedValues Result;
	Result.CloudCoverage = WeatherSimulationPrivate::SampleRange(Range.Minimum.CloudCoverage, Range.Maximum.CloudCoverage, RandomStream);
	Result.CloudDensity = WeatherSimulationPrivate::SampleRange(Range.Minimum.CloudDensity, Range.Maximum.CloudDensity, RandomStream);
	Result.Humidity = WeatherSimulationPrivate::SampleRange(Range.Minimum.Humidity, Range.Maximum.Humidity, RandomStream);
	Result.TemperatureCelsius = WeatherSimulationPrivate::SampleRange(Range.Minimum.TemperatureCelsius, Range.Maximum.TemperatureCelsius, RandomStream);
	Result.PressureHpa = WeatherSimulationPrivate::SampleRange(Range.Minimum.PressureHpa, Range.Maximum.PressureHpa, RandomStream);
	Result.Storminess = WeatherSimulationPrivate::SampleRange(Range.Minimum.Storminess, Range.Maximum.Storminess, RandomStream);
	Result.RainIntensity = WeatherSimulationPrivate::SampleRange(Range.Minimum.RainIntensity, Range.Maximum.RainIntensity, RandomStream);
	Result.LightningPotential = WeatherSimulationPrivate::SampleRange(Range.Minimum.LightningPotential, Range.Maximum.LightningPotential, RandomStream);
	return Result;
}

double FWeatherSimulationMath::SampleGeneratedSigma(
	const FWeatherSimulationSettings& Settings,
	const FWeatherGridInfo& GridInfo,
	FRandomStream& RandomStream)
{
	FVector2D Range = Settings.GeneratedSigmaRange;
	if (Settings.bUseCellRelativeGeneratedSigma && GridInfo.CellSize > UE_DOUBLE_SMALL_NUMBER)
	{
		Range = Settings.GeneratedSigmaCellRange * GridInfo.CellSize;
	}

	return FMath::Max(
		static_cast<double>(WeatherSimulationPrivate::SampleRange(
			static_cast<float>(Range.X),
			static_cast<float>(Range.Y),
			RandomStream)),
		1.0);
}

FVector2D FWeatherSimulationMath::GenerateStratifiedPosition(
	const FBox& GridBounds,
	const int32 SeedIndex,
	const int32 SeedCount,
	FRandomStream& RandomStream)
{
	if (!GridBounds.IsValid || SeedCount <= 0)
	{
		return FVector2D::ZeroVector;
	}

	const int32 ColumnCount = FMath::Max(FMath::CeilToInt(FMath::Sqrt(static_cast<float>(SeedCount))), 1);
	const int32 RowCount = FMath::Max(FMath::DivideAndRoundUp(SeedCount, ColumnCount), 1);
	const int32 ClampedIndex = FMath::Clamp(SeedIndex, 0, SeedCount - 1);
	const int32 Column = ClampedIndex % ColumnCount;
	const int32 Row = ClampedIndex / ColumnCount;
	const double FractionX = (Column + RandomStream.FRandRange(0.2f, 0.8f)) / ColumnCount;
	const double FractionY = (Row + RandomStream.FRandRange(0.2f, 0.8f)) / RowCount;
	return FVector2D(
		FMath::Lerp(GridBounds.Min.X, GridBounds.Max.X, FractionX),
		FMath::Lerp(GridBounds.Min.Y, GridBounds.Max.Y, FractionY));
}

int32 FWeatherSimulationMath::SelectGeneratedPresetIndex(
	const FWeatherSimulationSettings& Settings,
	const int32 SeedIndex)
{
	if (Settings.WeatherTypePresets.IsEmpty())
	{
		return INDEX_NONE;
	}

	// This order makes the first four default-generated seeds visibly distinct:
	// grey overcast, blue rain, purple storm, and pale-cyan partial cloud.
	static constexpr EWeatherType PreferredTypes[] = {
		EWeatherType::Overcast,
		EWeatherType::Rain,
		EWeatherType::Storm,
		EWeatherType::PartlyCloudy,
		EWeatherType::HeavyRain,
		EWeatherType::Clear,
		EWeatherType::Custom
	};
	TArray<int32, TInlineAllocator<8>> OrderedIndices;
	for (const EWeatherType PreferredType : PreferredTypes)
	{
		for (int32 PresetIndex = 0; PresetIndex < Settings.WeatherTypePresets.Num(); ++PresetIndex)
		{
			if (Settings.WeatherTypePresets[PresetIndex].WeatherType == PreferredType
				&& !OrderedIndices.Contains(PresetIndex))
			{
				OrderedIndices.Add(PresetIndex);
				break;
			}
		}
	}
	for (int32 PresetIndex = 0; PresetIndex < Settings.WeatherTypePresets.Num(); ++PresetIndex)
	{
		if (!OrderedIndices.Contains(PresetIndex))
		{
			OrderedIndices.Add(PresetIndex);
		}
	}

	return OrderedIndices[FMath::Abs(SeedIndex) % OrderedIndices.Num()];
}

int32 FWeatherSimulationMath::CalculateLifecycleTargetCount(
	const FWeatherFrontLifecycleSettings& Settings,
	const FWeatherGridInfo& GridInfo,
	const int32 MaximumSeedCount)
{
	if (!Settings.bEnabled || !GridInfo.bIsValid || GridInfo.CellCount <= 0)
	{
		return 0;
	}

	const int32 Minimum = FMath::Max(Settings.MinimumFrontCount, 0);
	const int32 Maximum = FMath::Max(Settings.MaximumFrontCount, Minimum);
	const float CellsPerFront = FMath::Max(Settings.TargetCellsPerFront, 1.0f);
	const int32 AreaTarget = FMath::CeilToInt(
		static_cast<float>(GridInfo.CellCount) / CellsPerFront);
	return FMath::Clamp(
		FMath::Clamp(AreaTarget, Minimum, Maximum),
		0,
		FMath::Max(MaximumSeedCount, 0));
}

int32 FWeatherSimulationMath::SelectWeightedArchetypeIndex(
	const TArray<FWeatherFrontArchetype>& Archetypes,
	FRandomStream& RandomStream)
{
	double TotalWeight = 0.0;
	for (const FWeatherFrontArchetype& Archetype : Archetypes)
	{
		if (Archetype.bEnabled)
		{
			TotalWeight += FMath::Max(static_cast<double>(Archetype.SpawnWeight), 0.0);
		}
	}

	if (TotalWeight <= UE_DOUBLE_SMALL_NUMBER)
	{
		return INDEX_NONE;
	}

	const double Selection = RandomStream.FRand() * TotalWeight;
	double AccumulatedWeight = 0.0;
	int32 LastValidIndex = INDEX_NONE;
	for (int32 ArchetypeIndex = 0; ArchetypeIndex < Archetypes.Num(); ++ArchetypeIndex)
	{
		const FWeatherFrontArchetype& Archetype = Archetypes[ArchetypeIndex];
		const double Weight = Archetype.bEnabled
			? FMath::Max(static_cast<double>(Archetype.SpawnWeight), 0.0)
			: 0.0;
		if (Weight <= 0.0)
		{
			continue;
		}

		LastValidIndex = ArchetypeIndex;
		AccumulatedWeight += Weight;
		if (Selection < AccumulatedWeight)
		{
			return ArchetypeIndex;
		}
	}
	return LastValidIndex;
}

FVector2D FWeatherSimulationMath::GenerateUpwindBoundaryPosition(
	const FWeatherGridInfo& GridInfo,
	const FVector2D& WindDirection,
	const float InsetCellFraction,
	FRandomStream& RandomStream)
{
	if (!GridInfo.bIsValid || !GridInfo.GridBounds.IsValid)
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D Minimum(GridInfo.GridBounds.Min.X, GridInfo.GridBounds.Min.Y);
	const FVector2D Maximum(GridInfo.GridBounds.Max.X, GridInfo.GridBounds.Max.Y);
	const FVector2D Size = Maximum - Minimum;
	const double DesiredInset = GridInfo.CellSize
		* FMath::Clamp(static_cast<double>(InsetCellFraction), 0.0, 0.49);
	const double InsetX = FMath::Min(DesiredInset, Size.X * 0.49);
	const double InsetY = FMath::Min(DesiredInset, Size.Y * 0.49);
	const FVector2D Direction = WindDirection.GetSafeNormal(
		UE_SMALL_NUMBER,
		FVector2D(1.0, 0.0));

	if (FMath::Abs(Direction.X) >= FMath::Abs(Direction.Y))
	{
		return FVector2D(
			Direction.X >= 0.0 ? Minimum.X + InsetX : Maximum.X - InsetX,
			RandomStream.FRandRange(Minimum.Y + InsetY, Maximum.Y - InsetY));
	}

	return FVector2D(
		RandomStream.FRandRange(Minimum.X + InsetX, Maximum.X - InsetX),
		Direction.Y >= 0.0 ? Minimum.Y + InsetY : Maximum.Y - InsetY);
}

EWeatherType FWeatherSimulationMath::ClassifyBuiltIn(
	const FWeatherCellState& State,
	const EWeatherType CurrentType)
{
	if (State.bIsStorm)
	{
		return EWeatherType::Storm;
	}
	if (State.bIsRaining && State.RainIntensity >= 0.8f)
	{
		return EWeatherType::HeavyRain;
	}
	if (State.bIsRaining)
	{
		return EWeatherType::Rain;
	}
	if (State.CloudCoverage >= 0.75f
		|| (CurrentType == EWeatherType::Overcast && State.CloudCoverage >= 0.7f))
	{
		return EWeatherType::Overcast;
	}
	if (State.CloudCoverage >= 0.3f
		|| (CurrentType == EWeatherType::PartlyCloudy && State.CloudCoverage >= 0.25f))
	{
		return EWeatherType::PartlyCloudy;
	}
	return EWeatherType::Clear;
}
