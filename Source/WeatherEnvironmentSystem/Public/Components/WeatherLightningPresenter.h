// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "WeatherPresentation.h"
#include "WeatherLightningPresenter.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UWeatherStateSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FWeatherLightningStrikeSignature,
	FVector,
	Location,
	float,
	Intensity,
	FWeatherCellCoord,
	CellCoord);

/** Deterministic per-cell lightning scheduler and bounded one-shot Niagara adapter. */
UCLASS(ClassGroup = (Weather), BlueprintType, meta = (BlueprintSpawnableComponent))
class WEATHERENVIRONMENTSYSTEM_API UWeatherLightningPresenter : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeatherLightningPresenter();

	void Configure(
		UWeatherStateSubsystem* InWeatherSubsystem,
		const FWeatherLightningPresentationSettings& InSettings,
		const FWeatherNiagaraParameterNames& InParameterNames,
		int32 InEnvironmentSeed);
	void UpdatePresentation(float DeltaSeconds, TConstArrayView<FVector> ViewLocations);
	void ShutdownPresentation();

	UFUNCTION(BlueprintCallable, Category = "Weather|Lightning")
	void SetPresentationEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Weather|Lightning")
	bool IsPresentationEnabled() const { return Settings.bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Weather|Lightning")
	int32 GetScheduledCellCount() const { return CellTimers.Num(); }

	UFUNCTION(BlueprintPure, Category = "Weather|Lightning")
	int32 GetActiveEffectCount() const { return ActiveComponents.Num(); }

	UFUNCTION(BlueprintPure, Category = "Weather|Lightning")
	float GetLastThunderDelaySeconds() const { return LastThunderDelaySeconds; }

	/** Fires at strike start even if the optional Niagara visual is unavailable or its pool is full. */
	UPROPERTY(BlueprintAssignable, Category = "Weather|Lightning")
	FWeatherLightningStrikeSignature OnLightningStrike;

	/** Fires after distance / speed-of-sound delay and is intended for thunder audio. */
	UPROPERTY(BlueprintAssignable, Category = "Weather|Lightning")
	FWeatherLightningStrikeSignature OnThunderDue;

protected:
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	struct FCellTimer
	{
		float RemainingSeconds = 0.0f;
		int32 StrikeSequence = 0;
	};

	struct FPendingThunder
	{
		FVector Location = FVector::ZeroVector;
		float Intensity = 0.0f;
		FWeatherCellCoord CellCoord;
		float RemainingSeconds = 0.0f;
	};

	void UpdateThunder(float DeltaSeconds);
	void UpdateCellTimers(float DeltaSeconds, TConstArrayView<FVector> ViewLocations);
	bool TriggerStrike(
		const FWeatherCellCoord& CellCoord,
		const FWeatherCellState& State,
		int32 StrikeSequence,
		TConstArrayView<FVector> ViewLocations);
	bool ResolveStrikeLocation(
		const FWeatherCellCoord& CellCoord,
		int32 StrikeSequence,
		FVector& OutStrikeLocation) const;
	void SpawnLightningEffect(
		const FVector& StrikeLocation,
		float Intensity,
		const FWeatherCellCoord& CellCoord,
		const FWeatherCellState& State);

	UFUNCTION()
	void HandleNiagaraSystemFinished(UNiagaraComponent* FinishedComponent);

	FWeatherLightningPresentationSettings Settings;
	FWeatherNiagaraParameterNames ParameterNames;
	TWeakObjectPtr<UWeatherStateSubsystem> WeatherSubsystem;
	TMap<FWeatherCellCoord, FCellTimer> CellTimers;
	TArray<FPendingThunder> PendingThunder;
	int32 EnvironmentSeed = 0;
	float LastThunderDelaySeconds = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedLightningSystem;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> ActiveComponents;
};
