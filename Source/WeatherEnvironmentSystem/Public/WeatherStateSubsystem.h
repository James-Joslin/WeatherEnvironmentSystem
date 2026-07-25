// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "WeatherDateTime.h"
#include "WeatherStateSubsystem.generated.h"

class AWeatherEnvironmentController;

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

	UPROPERTY(BlueprintAssignable, Category = "Weather|Clock")
	FWeatherDateTimeChangedSignature OnMinuteChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weather|Clock")
	FWeatherDateTimeChangedSignature OnHourChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weather|Clock")
	FWeatherDateTimeChangedSignature OnDayChanged;

	bool RegisterController(AWeatherEnvironmentController* Controller);
	void UnregisterController(AWeatherEnvironmentController* Controller);
	AWeatherEnvironmentController* GetActiveController() const;

private:
	void SetNativeDateTime(const FDateTime& NewDateTime, bool bBroadcastBoundaries);
	void BroadcastBoundaryChanges(const FDateTime& Previous, const FDateTime& Current);

	FDateTime CurrentDateTime = FDateTime(2026, 6, 21, 8, 0, 0);
	double TimeScale = 60.0;
	bool bPaused = false;
	bool bInitialClockSettingsApplied = false;

	TWeakObjectPtr<AWeatherEnvironmentController> ActiveController;
};
