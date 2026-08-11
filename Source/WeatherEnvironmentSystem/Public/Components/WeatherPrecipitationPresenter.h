// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "WeatherPresentation.h"
#include "WeatherPrecipitationPresenter.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UWeatherStateSubsystem;

/** View-local, bounded Niagara rain presentation derived from the authoritative CPU grid. */
UCLASS(ClassGroup = (Weather), BlueprintType, meta = (BlueprintSpawnableComponent))
class WEATHERENVIRONMENTSYSTEM_API UWeatherPrecipitationPresenter : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeatherPrecipitationPresenter();

	void Configure(
		UWeatherStateSubsystem* InWeatherSubsystem,
		const FWeatherPrecipitationPresentationSettings& InSettings,
		const FWeatherNiagaraParameterNames& InParameterNames);
	void UpdatePresentation(float DeltaSeconds, TConstArrayView<FVector> ViewLocations);
	void ShutdownPresentation();

	UFUNCTION(BlueprintCallable, Category = "Weather|Precipitation")
	void SetPresentationEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Weather|Precipitation")
	bool IsPresentationEnabled() const { return Settings.bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Weather|Precipitation")
	int32 GetAllocatedComponentCount() const { return PoolComponents.Num(); }

	UFUNCTION(BlueprintPure, Category = "Weather|Precipitation")
	int32 GetActiveCellCount() const;

	UFUNCTION(BlueprintPure, Category = "Weather|Precipitation")
	bool IsPresentingCell(FWeatherCellCoord CellCoord) const;

protected:
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	struct FPoolEntry
	{
		TWeakObjectPtr<UNiagaraComponent> Component;
		FWeatherCellCoord CellCoord;
		FWeatherCellState State;
		float TransitionAlpha = 0.0f;
		bool bAssigned = false;
		bool bDesired = false;
	};

	void RefreshCandidates(TConstArrayView<FVector> ViewLocations);
	void ReconcileAssignments();
	FPoolEntry* CreatePoolEntry();
	void AssignEntry(FPoolEntry& Entry, const FWeatherPrecipitationCandidate& Candidate);
	void ApplyEntryParameters(FPoolEntry& Entry) const;
	const FWeatherPrecipitationCandidate* FindCandidate(FWeatherCellCoord CellCoord) const;

	FWeatherPrecipitationPresentationSettings Settings;
	FWeatherNiagaraParameterNames ParameterNames;
	TWeakObjectPtr<UWeatherStateSubsystem> WeatherSubsystem;
	TArray<FPoolEntry> PoolEntries;
	TArray<FWeatherPrecipitationCandidate> SelectedCandidates;
	float SelectionAccumulator = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedRainSystem;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> PoolComponents;
};
