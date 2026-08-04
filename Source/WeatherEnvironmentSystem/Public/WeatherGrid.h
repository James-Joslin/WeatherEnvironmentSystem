// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WeatherGrid.generated.h"

class ALandscapeProxy;
class UWorld;

UENUM(BlueprintType)
enum class EWeatherGridSourceMode : uint8
{
	Landscape UMETA(DisplayName = "Landscape"),
	ManualBounds UMETA(DisplayName = "Manual Bounds")
};

UENUM(BlueprintType)
enum class EWeatherType : uint8
{
	Clear,
	PartlyCloudy UMETA(DisplayName = "Partly Cloudy"),
	Overcast,
	Rain,
	HeavyRain UMETA(DisplayName = "Heavy Rain"),
	Storm,
	Custom
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherCellCoord
{
	GENERATED_BODY()

	FWeatherCellCoord() = default;
	FWeatherCellCoord(const int32 InX, const int32 InY)
		: X(InX)
		, Y(InY)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid")
	int32 X = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid")
	int32 Y = INDEX_NONE;

	bool operator==(const FWeatherCellCoord& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}

	bool operator!=(const FWeatherCellCoord& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FWeatherCellCoord& Coord)
	{
		return HashCombine(::GetTypeHash(Coord.X), ::GetTypeHash(Coord.Y));
	}
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherCellState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid")
	FVector WorldCenter = FVector::ZeroVector;

	/** The blend/debug radius. This never creates collision or a scene component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid", meta = (ClampMin = "0.0"))
	double InfluenceRadius = 0.0;

	/** Direction and speed in centimetres per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (Units = "cm/s"))
	FVector WindVector = FVector(500.0, 0.0, 0.0);

	/** Normalized instantaneous gust contribution used by foliage and field-texture alpha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WindGust = 0.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	EWeatherType WeatherType = EWeatherType::Clear;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	bool bIsRaining = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	bool bIsStorm = false;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherSample
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid")
	bool bIsValid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid")
	FWeatherCellCoord CellCoord;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid")
	FWeatherCellState State;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherGridDefinition
{
	GENERATED_BODY()

	FWeatherGridDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid")
	EWeatherGridSourceMode SourceMode = EWeatherGridSourceMode::Landscape;

	/** The target cell width and length in centimetres. The default is one kilometre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid", meta = (ClampMin = "1.0", UIMin = "1000.0", Units = "cm"))
	double CellSize = 100000.0;

	/** Used directly in Manual Bounds mode and as a fallback when a world has no landscapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid")
	FBox ManualBounds;

	/** Absolute world-space vertical range accepted by point and bounds queries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid", meta = (Units = "cm"))
	double VerticalQueryMinimum = -100000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid", meta = (Units = "cm"))
	double VerticalQueryMaximum = 100000.0;

	/** Floors the XY origin to the cell lattice so small source-bound changes retain cell identity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid")
	bool bSnapOriginToCellSize = true;

	/** Rebuild is rejected before allocation when this cap would be exceeded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid", meta = (ClampMin = "1", UIMax = "4096"))
	int32 MaximumCellCount = 4096;

	/** Initial values for newly created cells. Spatial fields are assigned by the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid")
	FWeatherCellState DefaultCellState;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherGridInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid")
	bool bIsValid = false;

	/** Original landscape/manual bounds before XY cell-lattice expansion. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid")
	FBox SourceBounds;

	/** Complete bounds covered by the allocated cells. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid")
	FBox GridBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid")
	FIntPoint Dimensions = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid", meta = (Units = "cm"))
	double CellSize = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather Grid")
	int32 CellCount = 0;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherGridDebugSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug")
	bool bDrawOnlyWhenSelected = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug")
	bool bDrawGridLines = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug")
	bool bDrawInfluenceSpheres = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug")
	bool bDrawWindArrows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug|Labels")
	bool bDrawCoordinates = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug|Labels")
	bool bDrawWeatherType = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug|Labels")
	bool bDrawWindSpeed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug|Labels")
	bool bDrawCloudCoverage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug|Labels")
	bool bDrawHumidity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug|Labels")
	bool bDrawTemperature = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug|Labels")
	bool bDrawPressure = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug|Labels")
	bool bDrawRainIntensity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug|Labels")
	bool bDrawStorminess = false;

	/** Zero draws every cell. Otherwise cells outside this XY distance from the active view are skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug", meta = (ClampMin = "0.0", Units = "cm"))
	double DrawDistance = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug", meta = (ClampMin = "0.0"))
	float LineThickness = 2.0f;

	/** Multiplies the stored cm/s vector to produce a debug arrow displacement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug", meta = (ClampMin = "0.0"))
	float WindArrowScale = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug", meta = (ClampMin = "1.0", Units = "cm"))
	float ArrowHeadSize = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather Grid Debug", meta = (ClampMin = "4", ClampMax = "64"))
	int32 SphereSegments = 16;
};

/**
 * Deterministic row-major weather grid. Cells are plain data and this type creates no UObjects,
 * actors, components, or collision primitives.
 */
class WEATHERENVIRONMENTSYSTEM_API FWeatherGrid
{
public:
	bool Rebuild(const FBox& SourceBounds, const FWeatherGridDefinition& Definition, FString* OutMessage = nullptr);
	void Clear();

	bool WorldToCell(const FVector& WorldLocation, FWeatherCellCoord& OutCell) const;
	bool CellToWorld(const FWeatherCellCoord& Cell, FVector& OutWorldLocation) const;
	bool IsValidCell(const FWeatherCellCoord& Cell) const;
	bool GetCellState(const FWeatherCellCoord& Cell, FWeatherCellState& OutState) const;
	FWeatherCellState* FindMutableCell(const FWeatherCellCoord& Cell);
	const FWeatherCellState* FindCell(const FWeatherCellCoord& Cell) const;
	FBox GetCellBounds(const FWeatherCellCoord& Cell) const;
	/** Spatially interpolates continuous fields while retaining categorical state from the containing cell. */
	FWeatherSample GetWeatherAtLocationBilinear(const FVector& WorldLocation) const;
	/** C++ snapshot overload used to interpolate between completed fixed simulation steps. */
	FWeatherSample GetWeatherAtLocationBilinear(
		const FVector& WorldLocation,
		const TArray<FWeatherCellState>& CellSnapshot) const;
	FWeatherSample GetWeatherAtLocation(const FVector& WorldLocation) const;
	void GetCellsIntersectingBounds(const FBox& WorldBounds, TArray<FWeatherCellCoord>& OutCells) const;

	const FWeatherGridDefinition& GetDefinition() const { return GridDefinition; }
	const FWeatherGridInfo& GetInfo() const { return GridInfo; }
	const TArray<FWeatherCellState>& GetCells() const { return Cells; }
	TArray<FWeatherCellState>& GetMutableCells() { return Cells; }

	static bool ResolveSourceBounds(
		UWorld* World,
		const FWeatherGridDefinition& Definition,
		const TArray<ALandscapeProxy*>& LandscapeSources,
		FBox& OutBounds,
		FString& OutMessage);

private:
	int32 ToIndex(const FWeatherCellCoord& Cell) const;

	FWeatherGridDefinition GridDefinition;
	FWeatherGridInfo GridInfo;
	TArray<FWeatherCellState> Cells;
};
