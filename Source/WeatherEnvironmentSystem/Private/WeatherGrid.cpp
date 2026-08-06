// Copyright James Joslin. All Rights Reserved.

#include "WeatherGrid.h"

#include "EngineUtils.h"
#include "LandscapeProxy.h"

namespace WeatherGridPrivate
{
	bool IsFiniteVector(const FVector& Vector)
	{
		return FMath::IsFinite(Vector.X)
			&& FMath::IsFinite(Vector.Y)
			&& FMath::IsFinite(Vector.Z);
	}

	bool IsFiniteBounds(const FBox& Bounds)
	{
		return Bounds.IsValid
			&& IsFiniteVector(Bounds.Min)
			&& IsFiniteVector(Bounds.Max)
			&& Bounds.Max.X > Bounds.Min.X
			&& Bounds.Max.Y > Bounds.Min.Y;
	}
}

FWeatherGridDefinition::FWeatherGridDefinition()
	: ManualBounds(
		FVector(-500000.0, -500000.0, -100000.0),
		FVector(500000.0, 500000.0, 100000.0))
{
}

bool FWeatherGrid::Rebuild(
	const FBox& SourceBounds,
	const FWeatherGridDefinition& Definition,
	FString* OutMessage)
{
	auto Reject = [OutMessage](const FString& Message)
	{
		if (OutMessage)
		{
			*OutMessage = Message;
		}
		return false;
	};

	if (!FMath::IsFinite(Definition.CellSize) || Definition.CellSize <= 0.0)
	{
		return Reject(TEXT("Weather grid cell size must be greater than zero."));
	}

	if (!WeatherGridPrivate::IsFiniteBounds(SourceBounds))
	{
		return Reject(TEXT("Weather grid source bounds must have a valid, non-zero XY extent."));
	}

	if (!FMath::IsFinite(Definition.VerticalQueryMinimum)
		|| !FMath::IsFinite(Definition.VerticalQueryMaximum)
		|| Definition.VerticalQueryMaximum <= Definition.VerticalQueryMinimum)
	{
		return Reject(TEXT("Weather grid vertical query maximum must be greater than its minimum."));
	}

	const double CellSize = Definition.CellSize;
	const double OriginX = Definition.bSnapOriginToCellSize
		? FMath::FloorToDouble(SourceBounds.Min.X / CellSize) * CellSize
		: SourceBounds.Min.X;
	const double OriginY = Definition.bSnapOriginToCellSize
		? FMath::FloorToDouble(SourceBounds.Min.Y / CellSize) * CellSize
		: SourceBounds.Min.Y;
	const double RequiredX = (SourceBounds.Max.X - OriginX) / CellSize;
	const double RequiredY = (SourceBounds.Max.Y - OriginY) / CellSize;
	const double WidthAsDouble = FMath::CeilToDouble(RequiredX);
	const double HeightAsDouble = FMath::CeilToDouble(RequiredY);

	if (!FMath::IsFinite(WidthAsDouble)
		|| !FMath::IsFinite(HeightAsDouble)
		|| WidthAsDouble < 1.0
		|| HeightAsDouble < 1.0
		|| WidthAsDouble > static_cast<double>(MAX_int32)
		|| HeightAsDouble > static_cast<double>(MAX_int32))
	{
		return Reject(TEXT("Weather grid dimensions are invalid or exceed the supported integer range."));
	}

	const int32 Width = static_cast<int32>(WidthAsDouble);
	const int32 Height = static_cast<int32>(HeightAsDouble);
	const int64 CellCount64 = static_cast<int64>(Width) * static_cast<int64>(Height);
	const int32 SafetyCap = FMath::Max(Definition.MaximumCellCount, 1);
	if (CellCount64 > static_cast<int64>(SafetyCap))
	{
		return Reject(FString::Printf(
			TEXT("Weather grid requires %lld cells, exceeding the configured safety cap of %d. Increase cell size or the cap before rebuilding."),
			CellCount64,
			SafetyCap));
	}

	const FWeatherGrid PreviousGrid = *this;
	FWeatherGridInfo NewInfo;
	NewInfo.bIsValid = true;
	NewInfo.SourceBounds = SourceBounds;
	NewInfo.SourceBounds.Min.Z = Definition.VerticalQueryMinimum;
	NewInfo.SourceBounds.Max.Z = Definition.VerticalQueryMaximum;
	NewInfo.Origin = FVector(OriginX, OriginY, Definition.VerticalQueryMinimum);
	NewInfo.Dimensions = FIntPoint(Width, Height);
	NewInfo.CellSize = CellSize;
	NewInfo.CellCount = static_cast<int32>(CellCount64);
	NewInfo.GridBounds = FBox(
		NewInfo.Origin,
		FVector(
			OriginX + static_cast<double>(Width) * CellSize,
			OriginY + static_cast<double>(Height) * CellSize,
			Definition.VerticalQueryMaximum));

	TArray<FWeatherCellState> NewCells;
	NewCells.SetNumUninitialized(NewInfo.CellCount);
	const double CenterZ = (Definition.VerticalQueryMinimum + Definition.VerticalQueryMaximum) * 0.5;
	const double InfluenceRadius = CellSize * UE_SQRT_2 * 0.5;
	const bool bCanPreserve = PreviousGrid.GridInfo.bIsValid
		&& FMath::IsNearlyEqual(PreviousGrid.GridInfo.CellSize, CellSize);

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 Index = Y * Width + X;
			const FVector Center(
				OriginX + (static_cast<double>(X) + 0.5) * CellSize,
				OriginY + (static_cast<double>(Y) + 0.5) * CellSize,
				CenterZ);
			FWeatherCellState State = Definition.DefaultCellState;

			if (bCanPreserve)
			{
				FWeatherCellCoord PreviousCoord;
				const FVector PreviousQueryPoint(
					Center.X,
					Center.Y,
					PreviousGrid.GridInfo.GridBounds.GetCenter().Z);
				if (PreviousGrid.WorldToCell(PreviousQueryPoint, PreviousCoord))
				{
					const FWeatherCellState* PreviousState = PreviousGrid.FindCell(PreviousCoord);
					if (PreviousState
						&& FMath::IsNearlyEqual(PreviousState->WorldCenter.X, Center.X)
						&& FMath::IsNearlyEqual(PreviousState->WorldCenter.Y, Center.Y))
					{
						State = *PreviousState;
					}
				}
			}

			State.WorldCenter = Center;
			State.InfluenceRadius = InfluenceRadius;
			NewCells[Index] = State;
		}
	}

	GridDefinition = Definition;
	GridInfo = NewInfo;
	Cells = MoveTemp(NewCells);
	if (OutMessage)
	{
		*OutMessage = FString::Printf(
			TEXT("Built %d x %d weather grid (%d cells)."),
			Width,
			Height,
			GridInfo.CellCount);
	}
	return true;
}

void FWeatherGrid::Clear()
{
	GridInfo = FWeatherGridInfo();
	Cells.Reset();
}

bool FWeatherGrid::WorldToCell(
	const FVector& WorldLocation,
	FWeatherCellCoord& OutCell) const
{
	OutCell = FWeatherCellCoord();
	if (!GridInfo.bIsValid
		|| !WeatherGridPrivate::IsFiniteVector(WorldLocation)
		|| WorldLocation.X < GridInfo.GridBounds.Min.X
		|| WorldLocation.Y < GridInfo.GridBounds.Min.Y
		|| WorldLocation.Z < GridInfo.GridBounds.Min.Z
		|| WorldLocation.X > GridInfo.GridBounds.Max.X
		|| WorldLocation.Y > GridInfo.GridBounds.Max.Y
		|| WorldLocation.Z > GridInfo.GridBounds.Max.Z)
	{
		return false;
	}

	const int32 X = FMath::Clamp(
		FMath::FloorToInt((WorldLocation.X - GridInfo.Origin.X) / GridInfo.CellSize),
		0,
		GridInfo.Dimensions.X - 1);
	const int32 Y = FMath::Clamp(
		FMath::FloorToInt((WorldLocation.Y - GridInfo.Origin.Y) / GridInfo.CellSize),
		0,
		GridInfo.Dimensions.Y - 1);
	OutCell = FWeatherCellCoord(X, Y);
	return true;
}

bool FWeatherGrid::CellToWorld(
	const FWeatherCellCoord& Cell,
	FVector& OutWorldLocation) const
{
	const FWeatherCellState* State = FindCell(Cell);
	if (!State)
	{
		OutWorldLocation = FVector::ZeroVector;
		return false;
	}

	OutWorldLocation = State->WorldCenter;
	return true;
}

bool FWeatherGrid::IsValidCell(const FWeatherCellCoord& Cell) const
{
	return GridInfo.bIsValid
		&& Cell.X >= 0
		&& Cell.Y >= 0
		&& Cell.X < GridInfo.Dimensions.X
		&& Cell.Y < GridInfo.Dimensions.Y;
}

bool FWeatherGrid::GetCellState(
	const FWeatherCellCoord& Cell,
	FWeatherCellState& OutState) const
{
	const FWeatherCellState* State = FindCell(Cell);
	if (!State)
	{
		OutState = FWeatherCellState();
		return false;
	}

	OutState = *State;
	return true;
}

FWeatherCellState* FWeatherGrid::FindMutableCell(const FWeatherCellCoord& Cell)
{
	const int32 Index = ToIndex(Cell);
	return Cells.IsValidIndex(Index) ? &Cells[Index] : nullptr;
}

const FWeatherCellState* FWeatherGrid::FindCell(const FWeatherCellCoord& Cell) const
{
	const int32 Index = ToIndex(Cell);
	return Cells.IsValidIndex(Index) ? &Cells[Index] : nullptr;
}

FBox FWeatherGrid::GetCellBounds(const FWeatherCellCoord& Cell) const
{
	if (!IsValidCell(Cell))
	{
		return FBox(ForceInit);
	}

	const FVector Minimum(
		GridInfo.Origin.X + static_cast<double>(Cell.X) * GridInfo.CellSize,
		GridInfo.Origin.Y + static_cast<double>(Cell.Y) * GridInfo.CellSize,
		GridInfo.GridBounds.Min.Z);
	return FBox(
		Minimum,
		FVector(
			Minimum.X + GridInfo.CellSize,
			Minimum.Y + GridInfo.CellSize,
			GridInfo.GridBounds.Max.Z));
}

FWeatherSample FWeatherGrid::GetWeatherAtLocation(const FVector& WorldLocation) const
{
	FWeatherSample Sample;
	if (!WorldToCell(WorldLocation, Sample.CellCoord))
	{
		return Sample;
	}

	Sample.bIsValid = GetCellState(Sample.CellCoord, Sample.State);
	return Sample;
}

FWeatherSample FWeatherGrid::GetWeatherAtLocationBilinear(const FVector& WorldLocation) const
{
	return GetWeatherAtLocationBilinear(WorldLocation, Cells);
}

FWeatherSample FWeatherGrid::GetWeatherAtLocationBilinear(
	const FVector& WorldLocation,
	const TArray<FWeatherCellState>& CellSnapshot) const
{
	FWeatherSample Sample;
	if (!WorldToCell(WorldLocation, Sample.CellCoord)
		|| CellSnapshot.Num() != GridInfo.CellCount)
	{
		return Sample;
	}

	const double GridX = (WorldLocation.X - GridInfo.Origin.X) / GridInfo.CellSize - 0.5;
	const double GridY = (WorldLocation.Y - GridInfo.Origin.Y) / GridInfo.CellSize - 0.5;
	const int32 RawX0 = FMath::FloorToInt(GridX);
	const int32 RawY0 = FMath::FloorToInt(GridY);
	const int32 X0 = FMath::Clamp(RawX0, 0, GridInfo.Dimensions.X - 1);
	const int32 Y0 = FMath::Clamp(RawY0, 0, GridInfo.Dimensions.Y - 1);
	const int32 X1 = FMath::Clamp(RawX0 + 1, 0, GridInfo.Dimensions.X - 1);
	const int32 Y1 = FMath::Clamp(RawY0 + 1, 0, GridInfo.Dimensions.Y - 1);
	const float AlphaX = static_cast<float>(FMath::Clamp(GridX - RawX0, 0.0, 1.0));
	const float AlphaY = static_cast<float>(FMath::Clamp(GridY - RawY0, 0.0, 1.0));

	auto GetSnapshotCell = [&CellSnapshot, this](const int32 X, const int32 Y) -> const FWeatherCellState&
	{
		return CellSnapshot[Y * GridInfo.Dimensions.X + X];
	};
	const FWeatherCellState& State00 = GetSnapshotCell(X0, Y0);
	const FWeatherCellState& State10 = GetSnapshotCell(X1, Y0);
	const FWeatherCellState& State01 = GetSnapshotCell(X0, Y1);
	const FWeatherCellState& State11 = GetSnapshotCell(X1, Y1);
	const FWeatherCellState& CategoricalState = CellSnapshot[
		Sample.CellCoord.Y * GridInfo.Dimensions.X + Sample.CellCoord.X];

	auto BilinearFloat = [AlphaX, AlphaY](
		const float A,
		const float B,
		const float C,
		const float D)
	{
		return FMath::Lerp(FMath::Lerp(A, B, AlphaX), FMath::Lerp(C, D, AlphaX), AlphaY);
	};
	auto BilinearVector = [AlphaX, AlphaY](
		const FVector& A,
		const FVector& B,
		const FVector& C,
		const FVector& D)
	{
		return FMath::Lerp(FMath::Lerp(A, B, AlphaX), FMath::Lerp(C, D, AlphaX), AlphaY);
	};

	Sample.State = CategoricalState;
	Sample.State.WindVector = BilinearVector(
		State00.WindVector,
		State10.WindVector,
		State01.WindVector,
		State11.WindVector);
	Sample.State.WindGust = BilinearFloat(
		State00.WindGust,
		State10.WindGust,
		State01.WindGust,
		State11.WindGust);
	Sample.State.CloudCoverage = BilinearFloat(
		State00.CloudCoverage,
		State10.CloudCoverage,
		State01.CloudCoverage,
		State11.CloudCoverage);
	Sample.State.CloudDensity = BilinearFloat(
		State00.CloudDensity,
		State10.CloudDensity,
		State01.CloudDensity,
		State11.CloudDensity);
	Sample.State.Humidity = BilinearFloat(
		State00.Humidity,
		State10.Humidity,
		State01.Humidity,
		State11.Humidity);
	Sample.State.TemperatureCelsius = BilinearFloat(
		State00.TemperatureCelsius,
		State10.TemperatureCelsius,
		State01.TemperatureCelsius,
		State11.TemperatureCelsius);
	Sample.State.PressureHpa = BilinearFloat(
		State00.PressureHpa,
		State10.PressureHpa,
		State01.PressureHpa,
		State11.PressureHpa);
	Sample.State.Storminess = BilinearFloat(
		State00.Storminess,
		State10.Storminess,
		State01.Storminess,
		State11.Storminess);
	Sample.State.RainIntensity = BilinearFloat(
		State00.RainIntensity,
		State10.RainIntensity,
		State01.RainIntensity,
		State11.RainIntensity);
	Sample.State.LightningPotential = BilinearFloat(
		State00.LightningPotential,
		State10.LightningPotential,
		State01.LightningPotential,
		State11.LightningPotential);
	Sample.bIsValid = true;
	return Sample;
}

void FWeatherGrid::GetCellsIntersectingBounds(
	const FBox& WorldBounds,
	TArray<FWeatherCellCoord>& OutCells) const
{
	OutCells.Reset();
	if (!GridInfo.bIsValid
		|| !WorldBounds.IsValid
		|| WorldBounds.Max.X < GridInfo.GridBounds.Min.X
		|| WorldBounds.Max.Y < GridInfo.GridBounds.Min.Y
		|| WorldBounds.Max.Z < GridInfo.GridBounds.Min.Z
		|| WorldBounds.Min.X > GridInfo.GridBounds.Max.X
		|| WorldBounds.Min.Y > GridInfo.GridBounds.Max.Y
		|| WorldBounds.Min.Z > GridInfo.GridBounds.Max.Z)
	{
		return;
	}

	const int32 MinimumX = FMath::Clamp(
		FMath::FloorToInt((WorldBounds.Min.X - GridInfo.Origin.X) / GridInfo.CellSize),
		0,
		GridInfo.Dimensions.X - 1);
	const int32 MinimumY = FMath::Clamp(
		FMath::FloorToInt((WorldBounds.Min.Y - GridInfo.Origin.Y) / GridInfo.CellSize),
		0,
		GridInfo.Dimensions.Y - 1);
	const int32 MaximumX = FMath::Clamp(
		FMath::FloorToInt((WorldBounds.Max.X - GridInfo.Origin.X) / GridInfo.CellSize),
		0,
		GridInfo.Dimensions.X - 1);
	const int32 MaximumY = FMath::Clamp(
		FMath::FloorToInt((WorldBounds.Max.Y - GridInfo.Origin.Y) / GridInfo.CellSize),
		0,
		GridInfo.Dimensions.Y - 1);

	OutCells.Reserve((MaximumX - MinimumX + 1) * (MaximumY - MinimumY + 1));
	for (int32 Y = MinimumY; Y <= MaximumY; ++Y)
	{
		for (int32 X = MinimumX; X <= MaximumX; ++X)
		{
			OutCells.Emplace(X, Y);
		}
	}
}

bool FWeatherGrid::ResolveSourceBounds(
	UWorld* World,
	const FWeatherGridDefinition& Definition,
	const TArray<ALandscapeProxy*>& LandscapeSources,
	FBox& OutBounds,
	FString& OutMessage)
{
	OutBounds = FBox(ForceInit);
	OutMessage.Reset();

	if (Definition.SourceMode == EWeatherGridSourceMode::ManualBounds)
	{
		OutBounds = Definition.ManualBounds;
		if (!WeatherGridPrivate::IsFiniteBounds(OutBounds))
		{
			OutMessage = TEXT("Manual weather grid bounds are invalid or have zero XY extent.");
			return false;
		}
		return true;
	}

	auto AddLandscapeBounds = [&OutBounds](ALandscapeProxy* Landscape)
	{
		if (!IsValid(Landscape))
		{
			return;
		}

		const FBox Bounds = Landscape->GetComponentsBoundingBox(true);
		if (WeatherGridPrivate::IsFiniteBounds(Bounds))
		{
			OutBounds += Bounds;
		}
	};

	if (!LandscapeSources.IsEmpty())
	{
		for (ALandscapeProxy* Landscape : LandscapeSources)
		{
			AddLandscapeBounds(Landscape);
		}
	}
	else if (World)
	{
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			AddLandscapeBounds(*It);
		}
	}

	if (WeatherGridPrivate::IsFiniteBounds(OutBounds))
	{
		return true;
	}

	if (WeatherGridPrivate::IsFiniteBounds(Definition.ManualBounds))
	{
		OutBounds = Definition.ManualBounds;
		OutMessage = TEXT("No valid landscape source was found; using the configured manual bounds fallback.");
		return true;
	}

	OutMessage = TEXT("No valid landscape source or manual bounds fallback was available.");
	return false;
}

int32 FWeatherGrid::ToIndex(const FWeatherCellCoord& Cell) const
{
	return IsValidCell(Cell) ? Cell.Y * GridInfo.Dimensions.X + Cell.X : INDEX_NONE;
}
