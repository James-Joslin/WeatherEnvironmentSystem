// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "WeatherDateTime.generated.h"

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherDateTime
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Time", meta = (ClampMin = "1", ClampMax = "9999"))
	int32 Year = 2026;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Time", meta = (ClampMin = "1", ClampMax = "12"))
	int32 Month = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Time", meta = (ClampMin = "1", ClampMax = "31"))
	int32 Day = 21;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Time", meta = (ClampMin = "0", ClampMax = "23"))
	int32 Hour = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Time", meta = (ClampMin = "0", ClampMax = "59"))
	int32 Minute = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Time", meta = (ClampMin = "0", ClampMax = "59"))
	int32 Second = 0;

	bool ToDateTime(FDateTime& OutDateTime) const;
	static FWeatherDateTime FromDateTime(const FDateTime& DateTime);

	bool IsValid() const;
	int32 GetDayOfYear() const;
	double GetNormalizedDayFraction() const;
	FString ToDisplayString(bool bIncludeSeconds = false) const;

	bool operator==(const FWeatherDateTime& Other) const;
	bool operator!=(const FWeatherDateTime& Other) const { return !(*this == Other); }
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherClockSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock")
	FWeatherDateTime StartDateTime;

	/** Number of in-world seconds advanced per real second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3600.0"))
	double TimeScale = 60.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock")
	bool bStartPaused = false;
};
