// Copyright James Joslin. All Rights Reserved.

#include "WeatherStateSubsystem.h"

#include "Actors/WeatherEnvironmentController.h"
#include "Engine/GameInstance.h"

void UWeatherStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CurrentDateTime = FDateTime(2026, 6, 21, 8, 0, 0);
	TimeScale = 60.0;
	bPaused = false;
	bInitialClockSettingsApplied = false;
	WeatherGrid.Clear();
	bLastGridBuildSucceeded = false;
	LastGridBuildMessage = TEXT("Grid has not been built.");
	ActiveController.Reset();
}

void UWeatherStateSubsystem::Deinitialize()
{
	WeatherGrid.Clear();
	ActiveController.Reset();
	Super::Deinitialize();
}

void UWeatherStateSubsystem::Tick(const float DeltaTime)
{
	if (!bPaused && TimeScale > 0.0 && DeltaTime > 0.0f)
	{
		AdvanceWorldSeconds(static_cast<double>(DeltaTime) * TimeScale);
	}
}

TStatId UWeatherStateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWeatherStateSubsystem, STATGROUP_Tickables);
}

UWorld* UWeatherStateSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

bool UWeatherStateSubsystem::IsTickable() const
{
	return !IsTemplate();
}

void UWeatherStateSubsystem::InitializeClock(
	const FWeatherClockSettings& Settings,
	const bool bForceReset)
{
	if (bInitialClockSettingsApplied && !bForceReset)
	{
		return;
	}

	FDateTime Start;
	if (!Settings.StartDateTime.ToDateTime(Start))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Weather clock rejected invalid start date/time; retaining %s."),
			*CurrentDateTime.ToString());
		return;
	}

	SetNativeDateTime(Start, bInitialClockSettingsApplied);
	TimeScale = FMath::Max(0.0, Settings.TimeScale);
	bPaused = Settings.bStartPaused;
	bInitialClockSettingsApplied = true;
}

bool UWeatherStateSubsystem::SetDateTime(const FWeatherDateTime& NewDateTime)
{
	FDateTime Native;
	if (!NewDateTime.ToDateTime(Native))
	{
		return false;
	}

	SetNativeDateTime(Native, true);
	bInitialClockSettingsApplied = true;
	return true;
}

void UWeatherStateSubsystem::AdvanceWorldSeconds(const double WorldSeconds)
{
	if (FMath::IsNearlyZero(WorldSeconds))
	{
		return;
	}

	const FDateTime Previous = CurrentDateTime;
	const FTimespan Delta = FTimespan::FromSeconds(WorldSeconds);

	const FDateTime Minimum = FDateTime::MinValue();
	const FDateTime Maximum = FDateTime::MaxValue();
	if (Delta.GetTicks() > 0 && CurrentDateTime > Maximum - Delta)
	{
		CurrentDateTime = Maximum;
	}
	else if (Delta.GetTicks() < 0 && CurrentDateTime < Minimum - Delta)
	{
		CurrentDateTime = Minimum;
	}
	else
	{
		CurrentDateTime += Delta;
	}

	BroadcastBoundaryChanges(Previous, CurrentDateTime);
}

void UWeatherStateSubsystem::SetTimeScale(const double NewTimeScale)
{
	TimeScale = FMath::Max(0.0, NewTimeScale);
}

void UWeatherStateSubsystem::SetClockPaused(const bool bNewPaused)
{
	bPaused = bNewPaused;
}

FWeatherDateTime UWeatherStateSubsystem::GetCurrentDateTime() const
{
	return FWeatherDateTime::FromDateTime(CurrentDateTime);
}

double UWeatherStateSubsystem::GetNormalizedDayFraction() const
{
	return GetCurrentDateTime().GetNormalizedDayFraction();
}

FString UWeatherStateSubsystem::GetClockDisplayString(const bool bIncludeSeconds) const
{
	return GetCurrentDateTime().ToDisplayString(bIncludeSeconds);
}

bool UWeatherStateSubsystem::RebuildGridFromLandscape(
	const FWeatherGridDefinition& Definition)
{
	return RebuildGridFromLandscapeSources(Definition, TArray<ALandscapeProxy*>());
}

bool UWeatherStateSubsystem::RebuildGridFromBounds(
	const FBox& WorldBounds,
	const FWeatherGridDefinition& Definition)
{
	FString Message;
	const bool bBuilt = WeatherGrid.Rebuild(WorldBounds, Definition, &Message);
	bLastGridBuildSucceeded = bBuilt;
	LastGridBuildMessage = Message;
	if (bBuilt)
	{
		UE_LOG(LogTemp, Display, TEXT("WeatherEnvironment: %s"), *Message);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("WeatherEnvironment: %s"), *Message);
	}
	return bBuilt;
}

bool UWeatherStateSubsystem::RebuildGridFromLandscapeSources(
	const FWeatherGridDefinition& Definition,
	const TArray<ALandscapeProxy*>& LandscapeSources)
{
	FBox SourceBounds(ForceInit);
	FString Message;
	if (!FWeatherGrid::ResolveSourceBounds(
		GetWorld(),
		Definition,
		LandscapeSources,
		SourceBounds,
		Message))
	{
		bLastGridBuildSucceeded = false;
		LastGridBuildMessage = Message;
		UE_LOG(LogTemp, Warning, TEXT("WeatherEnvironment: %s"), *Message);
		return false;
	}

	if (!Message.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("WeatherEnvironment: %s"), *Message);
	}

	return RebuildGridFromBounds(SourceBounds, Definition);
}

void UWeatherStateSubsystem::ClearGrid()
{
	WeatherGrid.Clear();
	bLastGridBuildSucceeded = false;
	LastGridBuildMessage = TEXT("Grid cleared. Adjust the grid definition if needed, then press Rebuild Grid.");
}

bool UWeatherStateSubsystem::WorldToCell(
	const FVector& WorldLocation,
	FWeatherCellCoord& OutCell) const
{
	return WeatherGrid.WorldToCell(WorldLocation, OutCell);
}

bool UWeatherStateSubsystem::CellToWorld(
	const FWeatherCellCoord Cell,
	FVector& OutWorldLocation) const
{
	return WeatherGrid.CellToWorld(Cell, OutWorldLocation);
}

bool UWeatherStateSubsystem::IsValidCell(const FWeatherCellCoord Cell) const
{
	return WeatherGrid.IsValidCell(Cell);
}

bool UWeatherStateSubsystem::GetCellState(
	const FWeatherCellCoord Cell,
	FWeatherCellState& OutState) const
{
	return WeatherGrid.GetCellState(Cell, OutState);
}

FWeatherSample UWeatherStateSubsystem::GetWeatherAtLocation(
	const FVector& WorldLocation) const
{
	return WeatherGrid.GetWeatherAtLocation(WorldLocation);
}

TArray<FWeatherCellState> UWeatherStateSubsystem::GetCellStatesForBounds(
	const FBox& WorldBounds) const
{
	TArray<FWeatherCellCoord> Coordinates;
	WeatherGrid.GetCellsIntersectingBounds(WorldBounds, Coordinates);

	TArray<FWeatherCellState> States;
	States.Reserve(Coordinates.Num());
	for (const FWeatherCellCoord& Coordinate : Coordinates)
	{
		if (const FWeatherCellState* State = WeatherGrid.FindCell(Coordinate))
		{
			States.Add(*State);
		}
	}
	return States;
}

bool UWeatherStateSubsystem::RegisterController(AWeatherEnvironmentController* Controller)
{
	if (!IsValid(Controller))
	{
		return false;
	}

	if (ActiveController.IsValid() && ActiveController.Get() != Controller)
	{
		return false;
	}

	ActiveController = Controller;
	return true;
}

void UWeatherStateSubsystem::UnregisterController(AWeatherEnvironmentController* Controller)
{
	if (ActiveController.Get() == Controller)
	{
		ActiveController.Reset();
	}
}

AWeatherEnvironmentController* UWeatherStateSubsystem::GetActiveController() const
{
	return ActiveController.Get();
}

void UWeatherStateSubsystem::SetNativeDateTime(
	const FDateTime& NewDateTime,
	const bool bBroadcastBoundaries)
{
	const FDateTime Previous = CurrentDateTime;
	CurrentDateTime = NewDateTime;

	if (bBroadcastBoundaries)
	{
		BroadcastBoundaryChanges(Previous, CurrentDateTime);
	}
}

void UWeatherStateSubsystem::BroadcastBoundaryChanges(
	const FDateTime& Previous,
	const FDateTime& Current)
{
	const FWeatherDateTime BlueprintDateTime = FWeatherDateTime::FromDateTime(Current);

	if (Previous.GetYear() != Current.GetYear()
		|| Previous.GetMonth() != Current.GetMonth()
		|| Previous.GetDay() != Current.GetDay())
	{
		OnDayChanged.Broadcast(BlueprintDateTime);
	}

	if (Previous.GetYear() != Current.GetYear()
		|| Previous.GetMonth() != Current.GetMonth()
		|| Previous.GetDay() != Current.GetDay()
		|| Previous.GetHour() != Current.GetHour())
	{
		OnHourChanged.Broadcast(BlueprintDateTime);
	}

	if (Previous.GetYear() != Current.GetYear()
		|| Previous.GetMonth() != Current.GetMonth()
		|| Previous.GetDay() != Current.GetDay()
		|| Previous.GetHour() != Current.GetHour()
		|| Previous.GetMinute() != Current.GetMinute())
	{
		OnMinuteChanged.Broadcast(BlueprintDateTime);
	}
}
