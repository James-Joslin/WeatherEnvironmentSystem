// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "WeatherGrid.h"
#include "WeatherPresentation.generated.h"

class UNiagaraSystem;

/** Niagara user-parameter contract shared by the Stage 5 presentation adapters. */
USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherNiagaraParameterNames
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	FName CellCenter = TEXT("User.WeatherCellCenter");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	FName CellExtent = TEXT("User.WeatherCellExtent");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	FName RainIntensity = TEXT("User.WeatherRainIntensity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	FName WindVector = TEXT("User.WeatherWindVector");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	FName SpawnRate = TEXT("User.WeatherSpawnRate");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	FName TransitionAlpha = TEXT("User.WeatherTransitionAlpha");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning")
	FName LightningTarget = TEXT("User.WeatherLightningTarget");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning")
	FName LightningIntensity = TEXT("User.WeatherLightningIntensity");
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherPrecipitationPresentationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation")
	bool bEnabled = true;

	/** Optional rain system. A missing asset disables only the Niagara rain adapter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation")
	TSoftObjectPtr<UNiagaraSystem> RainSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation", meta = (ClampMin = "0", ClampMax = "128"))
	int32 PoolSize = 16;

	/** Maximum XY distance from a local view, expressed in current weather-cell widths. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation", meta = (ClampMin = "0.0"))
	float PresentationRadiusInCells = 3.0f;

	/** Expands the Niagara coverage beyond every cell edge to hide adjacent-cell seams. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CellOverlapFraction = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation", meta = (ClampMin = "0.0"))
	float MaximumSpawnRate = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation|Transitions", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation|Transitions", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutSeconds = 0.75f;

	/** Re-evaluation cadence; active Niagara parameters and fades still update every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation", meta = (ClampMin = "0.01", Units = "s"))
	float SelectionUpdateIntervalSeconds = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation|Priority", meta = (ClampMin = "0.0"))
	float IntensityPriorityWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precipitation|Priority", meta = (ClampMin = "0.0"))
	float ProximityPriorityWeight = 1.0f;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherLightningPresentationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning")
	bool bEnabled = true;

	/** Optional non-looping strike system. Lightning delegates still fire when this asset is absent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning")
	TSoftObjectPtr<UNiagaraSystem> LightningSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning", meta = (ClampMin = "0", ClampMax = "32"))
	int32 MaximumActiveEffects = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning", meta = (ClampMin = "0.0"))
	float PresentationRadiusInCells = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumLightningPotential = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float MinimumStrikeIntervalSeconds = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float MaximumStrikeIntervalSeconds = 30.0f;

	/** Multiplies the randomized interval at the eligibility threshold; it approaches one at full potential. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Timing", meta = (ClampMin = "1.0"))
	float LowPotentialIntervalMultiplier = 2.0f;

	/** Extra deterministic salt, independent of the Stage 4 environment seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Timing")
	int32 RandomSeedSalt = 5179;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Placement", meta = (ClampMin = "0.0", ClampMax = "0.49"))
	float CellEdgeInsetFraction = 0.1f;

	/** Blends uniform XY samples toward a centre-weighted triangular distribution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CenterWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Trace")
	bool bTraceToGround = true;

	/** When false, a failed trace falls back to the weather grid's centre Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Trace", meta = (EditCondition = "bTraceToGround"))
	bool bRequireGroundTraceHit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Trace", meta = (EditCondition = "bTraceToGround"))
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Trace", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bTraceToGround"))
	float GroundTraceHeight = 250000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Trace", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bTraceToGround"))
	float GroundTraceDepth = 500000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnHeight = 150000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Thunder", meta = (ClampMin = "1.0", Units = "cm/s"))
	float SpeedOfSound = 34300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning|Thunder", meta = (ClampMin = "0.0", Units = "s"))
	float MaximumThunderDelaySeconds = 20.0f;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherPresentationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	FWeatherPrecipitationPresentationSettings Precipitation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	FWeatherLightningPresentationSettings Lightning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	FWeatherNiagaraParameterNames NiagaraParameters;
};

struct WEATHERENVIRONMENTSYSTEM_API FWeatherPrecipitationCandidate
{
	FWeatherCellCoord CellCoord;
	FWeatherCellState State;
	double DistanceSquared = 0.0;
	float Priority = 0.0f;
};

/** Deterministic, UObject-free Stage 5 selection/timing helpers used by runtime presenters and tests. */
class WEATHERENVIRONMENTSYSTEM_API FWeatherPresentationMath
{
public:
	static float AdvanceTransitionAlpha(
		float CurrentAlpha,
		bool bFadeIn,
		float DeltaSeconds,
		float FadeInSeconds,
		float FadeOutSeconds);

	static void SelectPrecipitationCells(
		const FWeatherGrid& Grid,
		TConstArrayView<FVector> ViewLocations,
		const FWeatherPrecipitationPresentationSettings& Settings,
		int32 MaximumResults,
		TArray<FWeatherPrecipitationCandidate>& OutCandidates);

	static bool IsCellWithinPresentationRange(
		const FBox& CellBounds,
		TConstArrayView<FVector> ViewLocations,
		double Range,
		double& OutDistanceSquared);

	static float CalculateLightningStrikeInterval(
		const FWeatherLightningPresentationSettings& Settings,
		int32 EnvironmentSeed,
		const FWeatherCellCoord& CellCoord,
		int32 StrikeSequence,
		float LightningPotential);

	/** Returns a deterministic 0-1 position inside the configured cell-edge inset. */
	static FVector2D CalculateLightningUnitOffset(
		const FWeatherLightningPresentationSettings& Settings,
		int32 EnvironmentSeed,
		const FWeatherCellCoord& CellCoord,
		int32 StrikeSequence);

	/** Applies the configured trace-failure policy without coupling tests to a physics world. */
	static bool ResolveGroundTraceResult(
		bool bTraceEnabled,
		bool bRequireHit,
		bool bHit,
		const FVector& FallbackLocation,
		const FVector& HitLocation,
		FVector& OutLocation);
};
