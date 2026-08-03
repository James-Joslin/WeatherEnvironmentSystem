// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "WeatherDateTime.h"
#include "WeatherGrid.h"
#include "WeatherWind.h"
#include "WeatherStateSubsystem.generated.h"

class AWeatherEnvironmentController;
class AWeatherWindDirector;
class ALandscapeProxy;
class UMaterialParameterCollection;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FWeatherDateTimeChangedSignature,
	FWeatherDateTime,
	DateTime);

UCLASS()
class WEATHERENVIRONMENTSYSTEM_API UWeatherStateSubsystem
	: public UGameInstanceSubsystem
	, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/**
	 * Applies session start settings once. Subsequent level controllers cannot reset a clock
	 * that has persisted through travel unless bForceReset is true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weather|Clock")
	void InitializeClock(const FWeatherClockSettings& Settings, bool bForceReset = false);

	UFUNCTION(BlueprintCallable, Category = "Weather|Clock")
	bool SetDateTime(const FWeatherDateTime& NewDateTime);

	/** Advances by signed in-world seconds, independent of TimeScale. */
	UFUNCTION(BlueprintCallable, Category = "Weather|Clock")
	void AdvanceWorldSeconds(double WorldSeconds);

	UFUNCTION(BlueprintCallable, Category = "Weather|Clock")
	void SetTimeScale(double NewTimeScale);

	UFUNCTION(BlueprintPure, Category = "Weather|Clock")
	double GetTimeScale() const { return TimeScale; }

	UFUNCTION(BlueprintCallable, Category = "Weather|Clock")
	void SetClockPaused(bool bNewPaused);

	UFUNCTION(BlueprintPure, Category = "Weather|Clock")
	bool IsClockPaused() const { return bPaused; }

	UFUNCTION(BlueprintPure, Category = "Weather|Clock")
	FWeatherDateTime GetCurrentDateTime() const;

	UFUNCTION(BlueprintPure, Category = "Weather|Clock")
	double GetNormalizedDayFraction() const;

	UFUNCTION(BlueprintPure, Category = "Weather|Clock")
	FString GetClockDisplayString(bool bIncludeSeconds = false) const;

	/** Discovers all landscape proxies in the active world and falls back to manual bounds. */
	UFUNCTION(BlueprintCallable, Category = "Weather|Grid")
	bool RebuildGridFromLandscape(const FWeatherGridDefinition& Definition);

	/** Builds directly from explicit XY bounds while retaining the definition's query range and cap. */
	UFUNCTION(BlueprintCallable, Category = "Weather|Grid")
	bool RebuildGridFromBounds(const FBox& WorldBounds, const FWeatherGridDefinition& Definition);

	UFUNCTION(BlueprintCallable, Category = "Weather|Grid")
	void ClearGrid();

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	bool WorldToCell(const FVector& WorldLocation, FWeatherCellCoord& OutCell) const;

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	bool CellToWorld(FWeatherCellCoord Cell, FVector& OutWorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	bool IsValidCell(FWeatherCellCoord Cell) const;

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	bool GetCellState(FWeatherCellCoord Cell, FWeatherCellState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	FWeatherSample GetWeatherAtLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	TArray<FWeatherCellState> GetCellStatesForBounds(const FBox& WorldBounds) const;

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	FWeatherGridInfo GetGridInfo() const { return WeatherGrid.GetInfo(); }

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	FWeatherGridDefinition GetGridDefinition() const { return WeatherGrid.GetDefinition(); }

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	bool WasLastGridBuildSuccessful() const { return bLastGridBuildSucceeded; }

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	FString GetLastGridBuildMessage() const { return LastGridBuildMessage; }

	/** Applies cadence, speed, noise, gust, texture, and MPC settings without rebuilding the grid. */
	UFUNCTION(BlueprintCallable, Category = "Weather|Wind")
	void ConfigureWind(const FWeatherWindSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Weather|Wind")
	void SetWindDirector(AWeatherWindDirector* NewWindDirector);

	UFUNCTION(BlueprintPure, Category = "Weather|Wind")
	AWeatherWindDirector* GetWindDirector() const;

	UFUNCTION(BlueprintCallable, Category = "Weather|Wind")
	void ForceWindUpdate();

	UFUNCTION(BlueprintPure, Category = "Weather|Wind")
	bool GetWindAtLocation(const FVector& WorldLocation, FVector& OutWindVector, float& OutGust) const;

	UFUNCTION(BlueprintPure, Category = "Weather|Wind")
	UTexture2D* GetWindFieldTexture() const { return WindFieldTexture; }

	/** XY is field world origin, ZW is complete field world size. */
	UFUNCTION(BlueprintPure, Category = "Weather|Wind")
	FLinearColor GetWindFieldOriginSize() const;

	UPROPERTY(BlueprintAssignable, Category = "Weather|Clock")
	FWeatherDateTimeChangedSignature OnMinuteChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weather|Clock")
	FWeatherDateTimeChangedSignature OnHourChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weather|Clock")
	FWeatherDateTimeChangedSignature OnDayChanged;

	bool RegisterController(AWeatherEnvironmentController* Controller);
	void UnregisterController(AWeatherEnvironmentController* Controller);
	AWeatherEnvironmentController* GetActiveController() const;
	bool RebuildGridFromLandscapeSources(
		const FWeatherGridDefinition& Definition,
		const TArray<ALandscapeProxy*>& LandscapeSources);
	const FWeatherGrid& GetWeatherGrid() const { return WeatherGrid; }
	FWeatherGrid& GetMutableWeatherGrid() { return WeatherGrid; }

private:
	void SetNativeDateTime(const FDateTime& NewDateTime, bool bBroadcastBoundaries);
	void BroadcastBoundaryChanges(const FDateTime& Previous, const FDateTime& Current);
	void TickWind(float DeltaTime);
	void StepWind(float StepSeconds);
	void EnsureWindFieldTexture();
	void UpdateWindFieldTexture();
	void PublishWindMaterialParameters();

	FDateTime CurrentDateTime = FDateTime(2026, 6, 21, 8, 0, 0);
	double TimeScale = 60.0;
	bool bPaused = false;
	bool bInitialClockSettingsApplied = false;
	FWeatherGrid WeatherGrid;
	bool bLastGridBuildSucceeded = false;
	FString LastGridBuildMessage = TEXT("Grid has not been built.");
	FWeatherWindSettings WindSettings;
	double WindSimulationTimeSeconds = 0.0;
	float WindSimulationAccumulator = 0.0f;
	bool bWindConfigured = false;
	bool bWindFieldDirty = false;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WindFieldTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> WindMaterialParameterCollection;

	TArray<FColor> LastWindFieldPixels;
	TWeakObjectPtr<AWeatherWindDirector> WindDirector;

	TWeakObjectPtr<AWeatherEnvironmentController> ActiveController;
};
