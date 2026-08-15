// Copyright James Joslin. All Rights Reserved.

#include "Components/WeatherPrecipitationPresenter.h"

#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "WeatherStateSubsystem.h"

UWeatherPrecipitationPresenter::UWeatherPrecipitationPresenter()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeatherPrecipitationPresenter::Configure(
	UWeatherStateSubsystem* InWeatherSubsystem,
	const FWeatherPrecipitationPresentationSettings& InSettings,
	const FWeatherNiagaraParameterNames& InParameterNames)
{
	ShutdownPresentation();
	WeatherSubsystem = InWeatherSubsystem;
	Settings = InSettings;
	Settings.PoolSize = FMath::Max(Settings.PoolSize, 0);
	Settings.PresentationRadiusInCells = FMath::Max(Settings.PresentationRadiusInCells, 0.0f);
	Settings.SelectionUpdateIntervalSeconds = FMath::Max(
		Settings.SelectionUpdateIntervalSeconds,
		0.01f);
	ParameterNames = InParameterNames;
	LoadedRainSystem = Settings.bEnabled ? Settings.RainSystem.LoadSynchronous() : nullptr;
	SelectionAccumulator = Settings.SelectionUpdateIntervalSeconds;
}

void UWeatherPrecipitationPresenter::UpdatePresentation(
	const float DeltaSeconds,
	const TConstArrayView<FVector> ViewLocations)
{
	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.0f);
	SelectionAccumulator += SafeDeltaSeconds;
	if (SelectionAccumulator >= Settings.SelectionUpdateIntervalSeconds)
	{
		RefreshCandidates(ViewLocations);
		SelectionAccumulator = FMath::Fmod(
			SelectionAccumulator,
			Settings.SelectionUpdateIntervalSeconds);
	}

	ReconcileAssignments();
	bool bReleasedAnEntry = false;
	for (FPoolEntry& Entry : PoolEntries)
	{
		if (!Entry.bAssigned)
		{
			continue;
		}

		Entry.TransitionAlpha = FWeatherPresentationMath::AdvanceTransitionAlpha(
			Entry.TransitionAlpha,
			Entry.bDesired,
			SafeDeltaSeconds,
			Settings.FadeInSeconds,
			Settings.FadeOutSeconds);
		ApplyEntryParameters(Entry);
		if (!Entry.bDesired && Entry.TransitionAlpha <= UE_SMALL_NUMBER)
		{
			if (UNiagaraComponent* Component = Entry.Component.Get())
			{
				Component->DeactivateImmediate();
			}
			Entry.bAssigned = false;
			Entry.CellCoord = FWeatherCellCoord();
			bReleasedAnEntry = true;
		}
	}

	if (bReleasedAnEntry)
	{
		ReconcileAssignments();
	}
}

void UWeatherPrecipitationPresenter::SetPresentationEnabled(const bool bEnabled)
{
	Settings.bEnabled = bEnabled;
	if (bEnabled && !LoadedRainSystem)
	{
		LoadedRainSystem = Settings.RainSystem.LoadSynchronous();
	}
	SelectionAccumulator = Settings.SelectionUpdateIntervalSeconds;
	if (!bEnabled)
	{
		SelectedCandidates.Reset();
	}
}

int32 UWeatherPrecipitationPresenter::GetActiveCellCount() const
{
	int32 Count = 0;
	for (const FPoolEntry& Entry : PoolEntries)
	{
		Count += Entry.bAssigned ? 1 : 0;
	}
	return Count;
}

bool UWeatherPrecipitationPresenter::IsPresentingCell(const FWeatherCellCoord CellCoord) const
{
	for (const FPoolEntry& Entry : PoolEntries)
	{
		if (Entry.bAssigned && Entry.CellCoord == CellCoord)
		{
			return true;
		}
	}
	return false;
}

void UWeatherPrecipitationPresenter::OnComponentDestroyed(const bool bDestroyingHierarchy)
{
	ShutdownPresentation();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UWeatherPrecipitationPresenter::ShutdownPresentation()
{
	for (UNiagaraComponent* Component : PoolComponents)
	{
		if (IsValid(Component))
		{
			Component->DeactivateImmediate();
			Component->DestroyComponent();
		}
	}
	PoolComponents.Reset();
	PoolEntries.Reset();
	SelectedCandidates.Reset();
	LoadedRainSystem = nullptr;
	WeatherSubsystem.Reset();
	SelectionAccumulator = 0.0f;
}

void UWeatherPrecipitationPresenter::RefreshCandidates(
	const TConstArrayView<FVector> ViewLocations)
{
	SelectedCandidates.Reset();
	if (!Settings.bEnabled
		|| !LoadedRainSystem
		|| Settings.PoolSize <= 0
		|| !WeatherSubsystem.IsValid())
	{
		return;
	}

	FWeatherPresentationMath::SelectPrecipitationCells(
		WeatherSubsystem->GetWeatherGrid(),
		ViewLocations,
		Settings,
		Settings.PoolSize,
		SelectedCandidates);
}

void UWeatherPrecipitationPresenter::ReconcileAssignments()
{
	for (FPoolEntry& Entry : PoolEntries)
	{
		if (!Entry.bAssigned)
		{
			continue;
		}

		if (const FWeatherPrecipitationCandidate* Candidate = FindCandidate(Entry.CellCoord))
		{
			Entry.State = Candidate->State;
			Entry.bDesired = true;
		}
		else
		{
			Entry.bDesired = false;
		}
	}

	for (const FWeatherPrecipitationCandidate& Candidate : SelectedCandidates)
	{
		bool bAlreadyAssigned = false;
		for (const FPoolEntry& Entry : PoolEntries)
		{
			if (Entry.bAssigned && Entry.CellCoord == Candidate.CellCoord)
			{
				bAlreadyAssigned = true;
				break;
			}
		}
		if (bAlreadyAssigned)
		{
			continue;
		}

		FPoolEntry* AvailableEntry = nullptr;
		for (FPoolEntry& Entry : PoolEntries)
		{
			if (!Entry.bAssigned)
			{
				AvailableEntry = &Entry;
				break;
			}
		}
		if (!AvailableEntry)
		{
			AvailableEntry = CreatePoolEntry();
		}
		if (!AvailableEntry)
		{
			break;
		}
		AssignEntry(*AvailableEntry, Candidate);
	}
}

UWeatherPrecipitationPresenter::FPoolEntry* UWeatherPrecipitationPresenter::CreatePoolEntry()
{
	if (!LoadedRainSystem || PoolEntries.Num() >= Settings.PoolSize)
	{
		return nullptr;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->GetRootComponent())
	{
		return nullptr;
	}

	UNiagaraComponent* Component = NewObject<UNiagaraComponent>(Owner, NAME_None, RF_Transient);
	if (!Component)
	{
		return nullptr;
	}

	Component->SetAutoActivate(false);
	Component->SetAutoDestroy(false);
	Component->SetAsset(LoadedRainSystem);
	Component->SetupAttachment(Owner->GetRootComponent());
	Owner->AddInstanceComponent(Component);
	Component->RegisterComponent();
	PoolComponents.Add(Component);
	FPoolEntry& Entry = PoolEntries.AddDefaulted_GetRef();
	Entry.Component = Component;
	return &Entry;
}

void UWeatherPrecipitationPresenter::AssignEntry(
	FPoolEntry& Entry,
	const FWeatherPrecipitationCandidate& Candidate)
{
	UNiagaraComponent* Component = Entry.Component.Get();
	if (!Component)
	{
		return;
	}

	Entry.CellCoord = Candidate.CellCoord;
	Entry.State = Candidate.State;
	Entry.TransitionAlpha = 0.0f;
	Entry.bAssigned = true;
	Entry.bDesired = true;
	Component->SetWorldLocation(Candidate.State.WorldCenter);
	ApplyEntryParameters(Entry);
	Component->Activate(true);
}

void UWeatherPrecipitationPresenter::ApplyEntryParameters(FPoolEntry& Entry) const
{
	UNiagaraComponent* Component = Entry.Component.Get();
	if (!Component || !Entry.bAssigned || !WeatherSubsystem.IsValid())
	{
		return;
	}

	const double CellSize = WeatherSubsystem->GetGridInfo().CellSize;
	const double HalfXYExtent = CellSize * (0.5 + FMath::Max(Settings.CellOverlapFraction, 0.0f));
	const FWeatherGridDefinition Definition = WeatherSubsystem->GetGridDefinition();
	const float HalfZExtent = static_cast<float>(FMath::Max(
		(Definition.VerticalQueryMaximum - Definition.VerticalQueryMinimum) * 0.5,
		1.0));
	Component->SetWorldLocation(Entry.State.WorldCenter);
	if (!ParameterNames.CellCenter.IsNone())
	{
		Component->SetVariableVec3(ParameterNames.CellCenter, Entry.State.WorldCenter);
	}
	if (!ParameterNames.CellExtent.IsNone())
	{
		Component->SetVariableVec3(
			ParameterNames.CellExtent,
			FVector(HalfXYExtent, HalfXYExtent, HalfZExtent));
	}
	if (!ParameterNames.RainIntensity.IsNone())
	{
		Component->SetVariableFloat(
			ParameterNames.RainIntensity,
			FMath::Clamp(Entry.State.RainIntensity, 0.0f, 1.0f));
	}
	if (!ParameterNames.WindVector.IsNone())
	{
		Component->SetVariableVec3(ParameterNames.WindVector, Entry.State.WindVector);
	}
	if (!ParameterNames.SpawnRate.IsNone())
	{
		Component->SetVariableFloat(
			ParameterNames.SpawnRate,
			FMath::Max(Settings.MaximumSpawnRate, 0.0f)
				* FMath::Clamp(Entry.State.RainIntensity, 0.0f, 1.0f));
	}
	if (!ParameterNames.TransitionAlpha.IsNone())
	{
		Component->SetVariableFloat(ParameterNames.TransitionAlpha, Entry.TransitionAlpha);
	}
}

const FWeatherPrecipitationCandidate* UWeatherPrecipitationPresenter::FindCandidate(
	const FWeatherCellCoord CellCoord) const
{
	return SelectedCandidates.FindByPredicate(
		[CellCoord](const FWeatherPrecipitationCandidate& Candidate)
		{
			return Candidate.CellCoord == CellCoord;
		});
}
