// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WeatherGrid.h"
#include "WeatherWind.generated.h"

class UMaterialParameterCollection;
class UTexture2D;

/**
 * Maps the controller's physical wind into the parameter names already used by
 * the project's established tree, leaf, grass, and plant material functions.
 * Defaults reproduce Foliage_EnvironmentSettings at a 500 cm/s reference wind.
 */
USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherFoliageMaterialSettings
{
	GENERATED_BODY()

	/** Publish the compatibility parameters alongside the Stage 3 WeatherLocal values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
	bool bPublishCompatibilityParameters = true;

	/** Physical wind speed that reproduces the legacy MPC's authored defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (ClampMin = "1.0", Units = "cm/s"))
	float ReferenceWindSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Simple Wind", meta = (ClampMin = "0.0"))
	float SimpleWindIntensity = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Simple Wind", meta = (ClampMin = "0.0"))
	float SimpleWindSpeed = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Tree Sway", meta = (ClampMin = "0.0"))
	float WindSwayIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Tree Sway", meta = (ClampMin = "0.0", Units = "Hz"))
	float WindSwayGustFrequency = 0.2f;

	/** Horizontal component of the rotation axis consumed by MF_FoliageWind_Sway. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Tree Sway")
	float WindSwayAxisHorizontal = 0.947775f;

	/** Vertical rotation-axis component retained for parity with the authored MPC. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Tree Sway")
	float WindSwayAxisVertical = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Tree Sway")
	float WindSwayGradient = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Tree Sway")
	float WindSwayOffset = 0.992f;

	/** Adds animation-rate response for compatibility-only consumers. Migrated spatial legacy graphs keep authored rates phase-stable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (ClampMin = "0.0"))
	float GustAnimationResponse = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Plants", meta = (ClampMin = "1.0", Units = "cm"))
	float GrassWindSmallSize = 1024.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Plants", meta = (ClampMin = "1.0", Units = "cm"))
	float GrassWindLargeSize = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Plants", meta = (Units = "cm"))
	float GrassWindSmallAmplification = -70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Plants", meta = (Units = "cm"))
	float GrassWindLargeAmplification = -150.0f;
};

UENUM(BlueprintType)
enum class EWeatherWindDirectorMode : uint8
{
	Manual UMETA(DisplayName = "Manual Transform"),
	SplineRoute UMETA(DisplayName = "Spline Route")
};

UENUM(BlueprintType)
enum class EWeatherWindRouteBehavior : uint8
{
	Once,
	Loop,
	PingPong UMETA(DisplayName = "Ping Pong")
};

UENUM(BlueprintType)
enum class EWeatherWindRouteEasing : uint8
{
	Linear,
	EaseIn UMETA(DisplayName = "Ease In"),
	EaseOut UMETA(DisplayName = "Ease Out"),
	EaseInOut UMETA(DisplayName = "Ease In Out")
};

/** One world-space destination on a wind-director route. */
USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherWindRoutePoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Route")
	FVector Location = FVector::ZeroVector;

	/** Speed used while travelling to this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Route", meta = (ClampMin = "1.0", Units = "cm/s"))
	float TravelSpeed = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Route", meta = (ClampMin = "0.0", Units = "cm"))
	float ArrivalRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Route", meta = (ClampMin = "0.0", Units = "s"))
	float PauseDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Route")
	EWeatherWindRouteEasing Easing = EWeatherWindRouteEasing::EaseInOut;
};

/** Serializable route progress. Stage 8 can move this logical state into session persistence. */
USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherWindRouteState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind Route")
	bool bIsRunning = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind Route")
	bool bIsPaused = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind Route")
	int32 TargetPointIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind Route")
	int32 TravelDirection = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wind Route", meta = (Units = "s"))
	float PauseRemainingSeconds = 0.0f;

	FVector SegmentStart = FVector::ZeroVector;
	float SegmentElapsedSeconds = 0.0f;
	float SegmentDurationSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherWindSettings
{
	GENERATED_BODY()

	FWeatherWindSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	bool bEnabled = true;

	/** Authoritative grid wind updates use this fixed real-time cadence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0.01", Units = "s"))
	float FixedUpdateIntervalSeconds = 1.0f / 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0.0", Units = "cm/s"))
	float BaseWindSpeed = 500.0f;

	/** Used to normalize the B channel of the wind field texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "1.0", Units = "cm/s"))
	float MaximumWindSpeed = 2000.0f;

	/** Inside this XY radius, the previous valid direction is retained. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0.0", Units = "cm"))
	float DirectorDeadZoneRadius = 10000.0f;

	/** Exponential response rate. Zero applies target wind immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0.0"))
	float DirectionSmoothingRate = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	FVector DefaultDirection = FVector::ForwardVector;

	/** Optional large-scale angular perturbation, expressed as a 0-1 blend toward a curl vector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Curl", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CurlNoiseStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Curl", meta = (ClampMin = "1.0", Units = "cm"))
	float CurlNoiseWorldScale = 500000.0f;

	/** Noise-coordinate scroll per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Curl")
	float CurlNoiseScrollSpeed = 0.02f;

	/** Maximum normalized gust value stored in each cell and texture alpha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gust", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GustStrength = 0.35f;

	/** Fractional speed increase at a gust value of one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gust", meta = (ClampMin = "0.0"))
	float GustSpeedMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gust", meta = (ClampMin = "0.0", Units = "Hz"))
	float GustFrequency = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gust", meta = (ClampMin = "1.0", Units = "cm"))
	float GustWorldScale = 250000.0f;

	/** Shared texture sampled by MF_WeatherFoliageWind. It is updated in place, never per foliage MID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Materials", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> FieldTexture;

	/** Holds field mapping and player-local fallback values. MPCs cannot contain texture parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Materials", meta = (AllowedClasses = "/Script/Engine.MaterialParameterCollection"))
	TSoftObjectPtr<UMaterialParameterCollection> MaterialParameterCollection;

	/** Controls the controller-driven aliases used by the existing foliage material graph. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Materials")
	FWeatherFoliageMaterialSettings FoliageMaterials;
};

/** Derived values written to MPC_WeatherEnvironment for the established foliage graph. */
struct WEATHERENVIRONMENTSYSTEM_API FWeatherFoliageMaterialState
{
	float GrassWindSmallSize = 1024.0f;
	float GrassWindLargeSize = 2000.0f;
	float GrassWindSmallAmplification = -70.0f;
	float GrassWindLargeAmplification = -150.0f;
	float SimpleWindIntensity = 0.75f;
	float SimpleWindSpeed = 0.4f;
	float WindSwayGradient = 0.0f;
	float WindSwayGustFrequency = 0.2f;
	float WindSwayIntensity = 1.0f;
	float WindSwayOffset = 0.992f;
	FLinearColor WindSwayDirection = FLinearColor(0.0f, 0.947775f, 1.0f, 0.0f);
};

/** Uniform values used to evaluate the legacy foliage response at a spatial field sample. */
struct WEATHERENVIRONMENTSYSTEM_API FWeatherFoliageSpatialMaterialState
{
	/** X maximum/reference speed, Y reserved gust response, Z/W sway-axis horizontal/vertical. */
	FLinearColor Mapping = FLinearColor(4.0f, 0.25f, 0.947775f, 1.0f);

	/** X/Y simple-wind intensity/speed, Z/W tree-sway intensity/frequency. */
	FLinearColor Base = FLinearColor(0.75f, 0.4f, 1.0f, 0.2f);

	/** X/Y small/large gust amplitude, Z/W small/large wavelength. */
	FLinearColor GustBase = FLinearColor(-70.0f, -150.0f, 1024.0f, 2000.0f);
};

/** Pure route math shared by the actor and automation tests. */
class WEATHERENVIRONMENTSYSTEM_API FWeatherWindRouteSolver
{
public:
	static bool Start(
		const FVector& CurrentLocation,
		const TArray<FWeatherWindRoutePoint>& RoutePoints,
		FWeatherWindRouteState& InOutState);
	static void Stop(FWeatherWindRouteState& InOutState);
	static FVector Advance(
		float DeltaSeconds,
		const TArray<FWeatherWindRoutePoint>& RoutePoints,
		EWeatherWindRouteBehavior Behavior,
		FWeatherWindRouteState& InOutState,
		const FVector& CurrentLocation);
	static float ApplyEasing(float Alpha, EWeatherWindRouteEasing Easing);

private:
	static void PrepareSegment(
		const FVector& CurrentLocation,
		const TArray<FWeatherWindRoutePoint>& RoutePoints,
		FWeatherWindRouteState& InOutState);
	static bool SelectNextTarget(
		const TArray<FWeatherWindRoutePoint>& RoutePoints,
		EWeatherWindRouteBehavior Behavior,
		FWeatherWindRouteState& InOutState);
};

/** Pure wind/field math shared by the subsystem, editor preview, and tests. */
class WEATHERENVIRONMENTSYSTEM_API FWeatherWindMath
{
public:
	static FVector ResolveBaseDirection(
		const FVector& CellCenter,
		const FVector& DirectorLocation,
		const FVector& DirectorForward,
		const FVector& PreviousWind,
		const FWeatherWindSettings& Settings);
	static FVector CalculateTargetWind(
		const FVector& CellCenter,
		const FVector& DirectorLocation,
		const FVector& DirectorForward,
		const FVector& PreviousWind,
		double SimulationTimeSeconds,
		const FWeatherWindSettings& Settings,
		float& OutGust);
	static FVector SmoothWind(
		const FVector& PreviousWind,
		const FVector& TargetWind,
		float DeltaSeconds,
		float SmoothingRate);
	static FVector2D WorldToFieldUV(const FVector& WorldLocation, const FWeatherGridInfo& GridInfo);
	static FColor EncodeFieldTexel(const FVector& WindVector, float Gust, float MaximumWindSpeed);
	static void DecodeFieldTexel(
		const FColor& Texel,
		float MaximumWindSpeed,
		FVector& OutWindVector,
		float& OutGust);
	static FWeatherFoliageMaterialState BuildFoliageMaterialState(
		const FVector& WindDirection,
		float WindSpeed,
		float Gust,
		const FWeatherFoliageMaterialSettings& Settings);
	static FWeatherFoliageSpatialMaterialState BuildFoliageSpatialMaterialState(
		float MaximumWindSpeed,
		const FWeatherFoliageMaterialSettings& Settings);
	/** Returns X displacement scale and Y animation scale, matching MF_WeatherWindSample. */
	static FVector2D EvaluateFoliageSpatialScales(
		float NormalizedWindSpeed,
		float Gust,
		const FLinearColor& Mapping);
};
