// Copyright James Joslin. All Rights Reserved.

#include "Components/WeatherLightningPresenter.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "WeatherStateSubsystem.h"

UWeatherLightningPresenter::UWeatherLightningPresenter()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeatherLightningPresenter::Configure(
	UWeatherStateSubsystem* InWeatherSubsystem,
	const FWeatherLightningPresentationSettings& InSettings,
	const FWeatherNiagaraParameterNames& InParameterNames,
	const int32 InEnvironmentSeed)
{
	ShutdownPresentation();
	WeatherSubsystem = InWeatherSubsystem;
	Settings = InSettings;
	Settings.MaximumActiveEffects = FMath::Max(Settings.MaximumActiveEffects, 0);
	Settings.PresentationRadiusInCells = FMath::Max(Settings.PresentationRadiusInCells, 0.0f);
	ParameterNames = InParameterNames;
	EnvironmentSeed = InEnvironmentSeed;
	LoadedLightningSystem = Settings.bEnabled
		? Settings.LightningSystem.LoadSynchronous()
		: nullptr;
}

void UWeatherLightningPresenter::UpdatePresentation(
	const float DeltaSeconds,
	const TConstArrayView<FVector> ViewLocations)
{
	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
	UpdateThunder(SafeDeltaSeconds);
	if (!Settings.bEnabled || !WeatherSubsystem.IsValid() || ViewLocations.IsEmpty())
	{
		CellTimers.Reset();
		return;
	}
	UpdateCellTimers(SafeDeltaSeconds, ViewLocations);
}

void UWeatherLightningPresenter::SetPresentationEnabled(const bool bEnabled)
{
	Settings.bEnabled = bEnabled;
	CellTimers.Reset();
	if (bEnabled && !LoadedLightningSystem)
	{
		LoadedLightningSystem = Settings.LightningSystem.LoadSynchronous();
	}
}

void UWeatherLightningPresenter::OnComponentDestroyed(const bool bDestroyingHierarchy)
{
	ShutdownPresentation();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UWeatherLightningPresenter::ShutdownPresentation()
{
	for (UNiagaraComponent* Component : ActiveComponents)
	{
		if (IsValid(Component))
		{
			Component->OnSystemFinished.RemoveDynamic(
				this,
				&UWeatherLightningPresenter::HandleNiagaraSystemFinished);
			Component->DeactivateImmediate();
			Component->DestroyComponent();
		}
	}
	ActiveComponents.Reset();
	CellTimers.Reset();
	PendingThunder.Reset();
	LoadedLightningSystem = nullptr;
	WeatherSubsystem.Reset();
	LastThunderDelaySeconds = 0.0f;
}

void UWeatherLightningPresenter::UpdateThunder(const float DeltaSeconds)
{
	for (int32 ThunderIndex = PendingThunder.Num() - 1; ThunderIndex >= 0; --ThunderIndex)
	{
		FPendingThunder& Thunder = PendingThunder[ThunderIndex];
		Thunder.RemainingSeconds -= DeltaSeconds;
		if (Thunder.RemainingSeconds <= 0.0f)
		{
			OnThunderDue.Broadcast(Thunder.Location, Thunder.Intensity, Thunder.CellCoord);
			PendingThunder.RemoveAtSwap(ThunderIndex, 1, EAllowShrinking::No);
		}
	}
}

void UWeatherLightningPresenter::UpdateCellTimers(
	const float DeltaSeconds,
	const TConstArrayView<FVector> ViewLocations)
{
	const FWeatherGrid& Grid = WeatherSubsystem->GetWeatherGrid();
	const FWeatherGridInfo& Info = Grid.GetInfo();
	if (!Info.bIsValid)
	{
		CellTimers.Reset();
		return;
	}

	const double Range = static_cast<double>(Settings.PresentationRadiusInCells) * Info.CellSize;
	const float PotentialThreshold = FMath::Clamp(Settings.MinimumLightningPotential, 0.0f, 1.0f);
	TSet<FWeatherCellCoord> EligibleCells;
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

	TArray<FWeatherCellCoord> SortedNearbyCells = NearbyCells.Array();
	SortedNearbyCells.Sort([](const FWeatherCellCoord& A, const FWeatherCellCoord& B)
	{
		return A.Y == B.Y ? A.X < B.X : A.Y < B.Y;
	});
	for (const FWeatherCellCoord& Coord : SortedNearbyCells)
	{
		const FWeatherCellState* State = Grid.FindCell(Coord);
		if (!State || !State->bIsStorm || State->LightningPotential < PotentialThreshold)
		{
			continue;
		}

		double DistanceSquared = 0.0;
		if (!FWeatherPresentationMath::IsCellWithinPresentationRange(
			Grid.GetCellBounds(Coord),
			ViewLocations,
			Range,
			DistanceSquared))
		{
			continue;
		}

		EligibleCells.Add(Coord);
		FCellTimer* Timer = CellTimers.Find(Coord);
		if (!Timer)
		{
			FCellTimer NewTimer;
			NewTimer.RemainingSeconds = FWeatherPresentationMath::CalculateLightningStrikeInterval(
				Settings,
				EnvironmentSeed,
				Coord,
				0,
				State->LightningPotential);
			Timer = &CellTimers.Add(Coord, NewTimer);
		}

		Timer->RemainingSeconds -= DeltaSeconds;
		if (Timer->RemainingSeconds <= 0.0f)
		{
			TriggerStrike(Coord, *State, Timer->StrikeSequence, ViewLocations);
			++Timer->StrikeSequence;
			Timer->RemainingSeconds = FWeatherPresentationMath::CalculateLightningStrikeInterval(
				Settings,
				EnvironmentSeed,
				Coord,
				Timer->StrikeSequence,
				State->LightningPotential);
		}
	}

	for (auto TimerIt = CellTimers.CreateIterator(); TimerIt; ++TimerIt)
	{
		if (!EligibleCells.Contains(TimerIt.Key()))
		{
			TimerIt.RemoveCurrent();
		}
	}
}

bool UWeatherLightningPresenter::TriggerStrike(
	const FWeatherCellCoord& CellCoord,
	const FWeatherCellState& State,
	const int32 StrikeSequence,
	const TConstArrayView<FVector> ViewLocations)
{
	FVector StrikeLocation;
	if (!ResolveStrikeLocation(CellCoord, StrikeSequence, StrikeLocation))
	{
		return false;
	}

	const float Intensity = FMath::Clamp(
		FMath::Max(State.LightningPotential, State.Storminess),
		0.0f,
		1.0f);
	OnLightningStrike.Broadcast(StrikeLocation, Intensity, CellCoord);

	double NearestDistance = TNumericLimits<double>::Max();
	for (const FVector& ViewLocation : ViewLocations)
	{
		NearestDistance = FMath::Min(NearestDistance, FVector::Distance(ViewLocation, StrikeLocation));
	}
	LastThunderDelaySeconds = FMath::Clamp(
		static_cast<float>(NearestDistance / FMath::Max(Settings.SpeedOfSound, 1.0f)),
		0.0f,
		FMath::Max(Settings.MaximumThunderDelaySeconds, 0.0f));
	if (LastThunderDelaySeconds <= UE_SMALL_NUMBER)
	{
		OnThunderDue.Broadcast(StrikeLocation, Intensity, CellCoord);
	}
	else
	{
		FPendingThunder& Thunder = PendingThunder.AddDefaulted_GetRef();
		Thunder.Location = StrikeLocation;
		Thunder.Intensity = Intensity;
		Thunder.CellCoord = CellCoord;
		Thunder.RemainingSeconds = LastThunderDelaySeconds;
	}

	SpawnLightningEffect(StrikeLocation, Intensity, CellCoord, State);
	return true;
}

bool UWeatherLightningPresenter::ResolveStrikeLocation(
	const FWeatherCellCoord& CellCoord,
	const int32 StrikeSequence,
	FVector& OutStrikeLocation) const
{
	if (!WeatherSubsystem.IsValid())
	{
		return false;
	}

	const FWeatherGrid& Grid = WeatherSubsystem->GetWeatherGrid();
	const FBox CellBounds = Grid.GetCellBounds(CellCoord);
	if (!CellBounds.IsValid)
	{
		return false;
	}
	const FVector2D UnitOffset = FWeatherPresentationMath::CalculateLightningUnitOffset(
		Settings,
		EnvironmentSeed,
		CellCoord,
		StrikeSequence);
	const double X = FMath::Lerp(CellBounds.Min.X, CellBounds.Max.X, static_cast<double>(UnitOffset.X));
	const double Y = FMath::Lerp(CellBounds.Min.Y, CellBounds.Max.Y, static_cast<double>(UnitOffset.Y));
	const FVector FallbackLocation(X, Y, Grid.GetInfo().GridBounds.GetCenter().Z);
	OutStrikeLocation = FallbackLocation;
	if (!Settings.bTraceToGround)
	{
		return FWeatherPresentationMath::ResolveGroundTraceResult(
			false,
			Settings.bRequireGroundTraceHit,
			false,
			FallbackLocation,
			FallbackLocation,
			OutStrikeLocation);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return FWeatherPresentationMath::ResolveGroundTraceResult(
			true,
			Settings.bRequireGroundTraceHit,
			false,
			FallbackLocation,
			FallbackLocation,
			OutStrikeLocation);
	}
	const FVector TraceStart(X, Y, CellBounds.Max.Z + FMath::Max(Settings.GroundTraceHeight, 0.0f));
	const FVector TraceEnd(X, Y, CellBounds.Min.Z - FMath::Max(Settings.GroundTraceDepth, 0.0f));
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeatherLightningGroundTrace), true, GetOwner());
	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		Settings.GroundTraceChannel,
		QueryParams);
	return FWeatherPresentationMath::ResolveGroundTraceResult(
		true,
		Settings.bRequireGroundTraceHit,
		bHit,
		FallbackLocation,
		Hit.ImpactPoint,
		OutStrikeLocation);
}

void UWeatherLightningPresenter::SpawnLightningEffect(
	const FVector& StrikeLocation,
	const float Intensity,
	const FWeatherCellCoord& CellCoord,
	const FWeatherCellState& State)
{
	if (!LoadedLightningSystem
		|| ActiveComponents.Num() >= Settings.MaximumActiveEffects)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->GetRootComponent())
	{
		return;
	}

	UNiagaraComponent* Component = NewObject<UNiagaraComponent>(Owner, NAME_None, RF_Transient);
	if (!Component)
	{
		return;
	}
	Component->SetAutoActivate(false);
	Component->SetAutoDestroy(false);
	Component->SetAsset(LoadedLightningSystem);
	Component->SetupAttachment(Owner->GetRootComponent());
	Owner->AddInstanceComponent(Component);
	Component->RegisterComponent();
	Component->SetWorldLocation(
		StrikeLocation + FVector(0.0, 0.0, FMath::Max(Settings.SpawnHeight, 0.0f)));
	if (!ParameterNames.CellCenter.IsNone())
	{
		Component->SetVariableVec3(ParameterNames.CellCenter, State.WorldCenter);
	}
	if (!ParameterNames.LightningTarget.IsNone())
	{
		Component->SetVariableVec3(ParameterNames.LightningTarget, StrikeLocation);
	}
	if (!ParameterNames.LightningIntensity.IsNone())
	{
		Component->SetVariableFloat(ParameterNames.LightningIntensity, Intensity);
	}
	Component->OnSystemFinished.AddDynamic(
		this,
		&UWeatherLightningPresenter::HandleNiagaraSystemFinished);
	ActiveComponents.Add(Component);
	Component->Activate(true);
}

void UWeatherLightningPresenter::HandleNiagaraSystemFinished(UNiagaraComponent* FinishedComponent)
{
	if (!FinishedComponent)
	{
		return;
	}
	FinishedComponent->OnSystemFinished.RemoveDynamic(
		this,
		&UWeatherLightningPresenter::HandleNiagaraSystemFinished);
	ActiveComponents.RemoveSingleSwap(FinishedComponent, EAllowShrinking::No);
	FinishedComponent->DestroyComponent();
}
