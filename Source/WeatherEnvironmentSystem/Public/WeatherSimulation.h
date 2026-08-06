// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeatherGrid.h"
#include "WeatherSimulation.generated.h"

UENUM(BlueprintType)
enum class EWeatherSeedBoundaryPolicy : uint8
{
	Wrap,
	Clamp,
	Expire
};

UENUM(BlueprintType)
enum class EWeatherSeedValueSource : uint8
{
	Fixed,
	ProfileSampled UMETA(DisplayName = "Profile Sampled")
};

UENUM(BlueprintType)
enum class EWeatherBooleanRequirement : uint8
{
	Any,
	RequiredFalse UMETA(DisplayName = "Must Be False"),
	RequiredTrue UMETA(DisplayName = "Must Be True")
};

/** Weather values carried by a seed. Wind remains owned by the Stage 3 wind simulation. */
USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherSeedValues
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CloudCoverage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CloudDensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Humidity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (Units = "Celsius"))
	float TemperatureCelsius = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float PressureHpa = 1013.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Storminess = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RainIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LightningPotential = 0.0f;
};

/** Inclusive range used when a seed asks the environment profile to sample its values. */
USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherSeedValueRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FWeatherSeedValues Minimum;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FWeatherSeedValues Maximum;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherTypePreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	EWeatherType WeatherType = EWeatherType::Clear;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FWeatherSeedValues Values;
};

/** Weighted production recipe used when the lifecycle manager creates a moving front. */
USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherFrontArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Front")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Front")
	EWeatherType WeatherType = EWeatherType::PartlyCloudy;

	/** Relative selection weight. Zero prevents automatic spawning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Front", meta = (ClampMin = "0.0"))
	float SpawnWeight = 1.0f;

	/** Gaussian sigma in weather-cell widths. Influence ends at three sigma. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Front", meta = (ClampMin = "0.01"))
	FVector2D SigmaCellRange = FVector2D(0.6, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Front", meta = (ClampMin = "0.0"))
	FVector2D StrengthRange = FVector2D(0.75, 1.25);

	/** Real simulation seconds. Zero in either endpoint may produce an infinite front. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Front", meta = (Units = "s"))
	FVector2D LifetimeSecondsRange = FVector2D(900.0, 1800.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Front", meta = (ClampMin = "0.0"))
	FVector2D MovementMultiplierRange = FVector2D(0.75, 1.15);
};

/** Maintains a deterministic, area-scaled population of fronts during long sessions. */
USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherFrontLifecycleSettings
{
	GENERATED_BODY()

	FWeatherFrontLifecycleSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	bool bEnabled = true;

	/** Target density. Sixteen means approximately one active front per sixteen cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "1.0"))
	float TargetCellsPerFront = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "0"))
	int32 MinimumFrontCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "0"))
	int32 MaximumFrontCount = 32;

	/** Authored and Blueprint-added seeds reduce the number of automatically managed fronts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	bool bCountExternalSeedsTowardTarget = true;

	/** Real seconds between gradual population checks after the initial fill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "0.01", Units = "s"))
	float ReplenishmentIntervalSeconds = 30.0f;

	/** Prevents a large population correction from appearing in one running frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "1"))
	int32 MaximumSpawnsPerInterval = 2;

	/** Initial fill is spread through the world; later replacements enter from the upwind edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	bool bSpawnReplacementsAtUpwindBoundary = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "0.0", ClampMax = "0.49"))
	float BoundaryInsetCellFraction = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "0.0"))
	float MinimumSpacingCellWidths = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "1", ClampMax = "64"))
	int32 PositionAttempts = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	TArray<FWeatherFrontArchetype> Archetypes;
};

/** A compact persistent source for a coherent weather front. Sigma is its Gaussian standard deviation. */
USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherSeed
{
	GENERATED_BODY()

	/** Invalid IDs are replaced with deterministic IDs when the seed is added to the subsystem. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Seed")
	FGuid Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Seed", meta = (MakeEditWidget = "true", Units = "cm"))
	FVector2D Position = FVector2D::ZeroVector;

	/** Gaussian standard deviation in world centimetres. Evaluation is limited to three sigma. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Seed", meta = (ClampMin = "1.0", Units = "cm"))
	double Sigma = 200000.0;

	/** Multiplies this seed's Gaussian contribution before weights are normalized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Seed", meta = (ClampMin = "0.0"))
	float Strength = 1.0f;

	/** Zero or less means infinite lifetime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Seed", meta = (Units = "s"))
	float LifetimeSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Seed", meta = (Units = "s"))
	float AgeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Seed", meta = (ClampMin = "0.0"))
	float MovementMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Seed")
	EWeatherSeedValueSource ValueSource = EWeatherSeedValueSource::Fixed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Seed")
	FWeatherSeedValues Values;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Seed")
	bool bUseWeatherTypePreset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Seed", meta = (EditCondition = "bUseWeatherTypePreset"))
	EWeatherType WeatherTypePreset = EWeatherType::Clear;

	/** True only for fronts automatically created by the lifecycle manager. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Seed")
	bool bManagedByLifecycle = false;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherFloatRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule", meta = (EditCondition = "bEnabled"))
	float Minimum = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule", meta = (EditCondition = "bEnabled"))
	float Maximum = 1.0f;

	bool Contains(float Value, float Expansion = 0.0f) const;
};

/** Lightweight presentation metadata selected alongside a classified weather type. */
USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherTypePresentationProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	FLinearColor DebugColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation", meta = (ClampMin = "0.0"))
	float PrecipitationMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation", meta = (ClampMin = "0.0"))
	float CloudDarknessMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherTypeClassificationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	EWeatherType WeatherType = EWeatherType::Clear;

	/** Highest matching priority wins. Equal-priority ties retain the asset's order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	FWeatherFloatRange CloudCoverage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	FWeatherFloatRange CloudDensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	FWeatherFloatRange Humidity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	FWeatherFloatRange TemperatureCelsius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	FWeatherFloatRange PressureHpa;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	FWeatherFloatRange Storminess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	FWeatherFloatRange RainIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	FWeatherFloatRange LightningPotential;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	EWeatherBooleanRequirement IsRaining = EWeatherBooleanRequirement::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	EWeatherBooleanRequirement IsStorm = EWeatherBooleanRequirement::Any;

	/** Expands enabled ranges only while this is the cell's current type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule", meta = (ClampMin = "0.0"))
	float Hysteresis = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Rule")
	FWeatherTypePresentationProfile Presentation;

	bool Matches(const FWeatherCellState& State, bool bApplyHysteresis) const;
};

UCLASS(BlueprintType)
class WEATHERENVIRONMENTSYSTEM_API UWeatherTypeLookupDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Version")
	int32 DataVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Classification")
	TArray<FWeatherTypeClassificationRule> Rules;

	UFUNCTION(BlueprintPure, Category = "Weather|Classification")
	EWeatherType ClassifyWeather(const FWeatherCellState& State, EWeatherType CurrentType) const;

	UFUNCTION(BlueprintPure, Category = "Weather|Classification")
	bool GetPresentationProfile(
		EWeatherType WeatherType,
		FWeatherTypePresentationProfile& OutPresentation) const;

	const FWeatherTypeClassificationRule* FindRule(EWeatherType WeatherType) const;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherSimulationSettings
{
	GENERATED_BODY()

	FWeatherSimulationSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation", meta = (ClampMin = "0.001", Units = "s"))
	float FixedUpdateIntervalSeconds = 0.033333333f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaximumSubstepsPerFrame = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation", meta = (ClampMin = "0.0001"))
	float BaselineWeight = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	FWeatherSeedValues BaselineValues;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeds")
	EWeatherSeedBoundaryPolicy BoundaryPolicy = EWeatherSeedBoundaryPolicy::Wrap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeds")
	int32 EnvironmentSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeds", meta = (ClampMin = "1"))
	int32 MaximumSeedCount = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeds")
	TArray<FWeatherSeed> InitialSeeds;

	/** Legacy/manual initial generator. Leave at zero when Front Lifecycle owns population density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeds", meta = (ClampMin = "0"))
	int32 InitialGeneratedSeedCount = 0;

	/** Keeps generated front size useful when designers change weather-grid cell size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seed Generation")
	bool bUseCellRelativeGeneratedSigma = true;

	/** Gaussian sigma expressed in weather-cell widths. Influence ends at three sigma. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seed Generation", meta = (ClampMin = "0.01", EditCondition = "bUseCellRelativeGeneratedSigma"))
	FVector2D GeneratedSigmaCellRange = FVector2D(0.35, 0.65);

	/** Legacy/advanced absolute world range used when cell-relative sigma is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seed Generation|Advanced", meta = (Units = "cm", EditCondition = "!bUseCellRelativeGeneratedSigma"))
	FVector2D GeneratedSigmaRange = FVector2D(100000.0, 350000.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seed Generation")
	FVector2D GeneratedStrengthRange = FVector2D(0.5, 1.5);

	/** Zero in either endpoint is allowed and means infinite lifetime when selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seed Generation", meta = (Units = "s"))
	FVector2D GeneratedLifetimeRange = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seed Generation")
	FVector2D GeneratedMovementMultiplierRange = FVector2D(0.75, 1.25);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seed Generation")
	FWeatherSeedValueRange GeneratedValueRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seed Generation")
	TArray<FWeatherTypePreset> WeatherTypePresets;

	/** Optional production population manager. Manual seed APIs remain available while it is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Front Lifecycle")
	FWeatherFrontLifecycleSettings FrontLifecycle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RainEnterThreshold = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RainExitThreshold = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StormEnterThreshold = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StormExitThreshold = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Classification", meta = (ClampMin = "0.0", Units = "s"))
	float MinimumWeatherTypeDurationSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Classification")
	TObjectPtr<UWeatherTypeLookupDataAsset> WeatherTypeLookup = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events", meta = (ClampMin = "0.0"))
	float LocalWeatherChangeThreshold = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Events", meta = (ClampMin = "0.0", Units = "s"))
	float LocalWeatherEventDebounceSeconds = 0.25f;
};

/** Pure fixed-step helpers shared by the subsystem and Stage 4 automation tests. */
class WEATHERENVIRONMENTSYSTEM_API FWeatherSimulationMath
{
public:
	static FGuid MakeDeterministicSeedId(int32 EnvironmentSeed, int32 SeedSerial);
	static double GaussianWeight(double Distance, double Sigma, double Strength = 1.0);
	static bool ApplyBoundaryPolicy(
		FVector2D& InOutPosition,
		const FWeatherGridInfo& GridInfo,
		EWeatherSeedBoundaryPolicy BoundaryPolicy);
	static void AdvectSeeds(
		TArray<FWeatherSeed>& InOutSeeds,
		const FWeatherGrid& Grid,
		const FWeatherSimulationSettings& Settings,
		float StepSeconds);
	static void RebuildCellFields(
		FWeatherGrid& Grid,
		const TArray<FWeatherSeed>& Seeds,
		const FWeatherSimulationSettings& Settings,
		float StepSeconds,
		TArray<float>& InOutWeatherTypeDurations,
		int64* OutEvaluatedCellCount = nullptr);
	static FWeatherSeedValues SampleValues(
		const FWeatherSeedValueRange& Range,
		FRandomStream& RandomStream);
	static double SampleGeneratedSigma(
		const FWeatherSimulationSettings& Settings,
		const FWeatherGridInfo& GridInfo,
		FRandomStream& RandomStream);
	static FVector2D GenerateStratifiedPosition(
		const FBox& GridBounds,
		int32 SeedIndex,
		int32 SeedCount,
		FRandomStream& RandomStream);
	static int32 SelectGeneratedPresetIndex(
		const FWeatherSimulationSettings& Settings,
		int32 SeedIndex);
	static int32 CalculateLifecycleTargetCount(
		const FWeatherFrontLifecycleSettings& Settings,
		const FWeatherGridInfo& GridInfo,
		int32 MaximumSeedCount);
	static int32 SelectWeightedArchetypeIndex(
		const TArray<FWeatherFrontArchetype>& Archetypes,
		FRandomStream& RandomStream);
	static FVector2D GenerateUpwindBoundaryPosition(
		const FWeatherGridInfo& GridInfo,
		const FVector2D& WindDirection,
		float InsetCellFraction,
		FRandomStream& RandomStream);
	static EWeatherType ClassifyBuiltIn(
		const FWeatherCellState& State,
		EWeatherType CurrentType = EWeatherType::Clear);
};
