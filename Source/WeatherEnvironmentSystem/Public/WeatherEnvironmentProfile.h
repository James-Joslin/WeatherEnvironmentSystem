// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeatherDateTime.h"
#include "WeatherEnvironmentProfile.generated.h"

class UCurveFloat;
class UCurveLinearColor;
class UMaterialInterface;
class UStaticMesh;
class UTextureCube;

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherAstronomySettings
{
	GENERATED_BODY()

	/** Latitude in degrees. Positive values are north of the equator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	double LatitudeDegrees = 35.0;

	/** Longitude in degrees. Positive values are east of Greenwich. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	double LongitudeDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy", meta = (ClampMin = "-14.0", ClampMax = "14.0"))
	double UTCOffsetHours = 0.0;

	/** Rotates astronomical north around Unreal world +Z. Zero means world +X is north. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy", meta = (UIMin = "-180.0", UIMax = "180.0"))
	double NorthYawDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy")
	bool bDriveSunLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy")
	bool bDriveMoonLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy")
	bool bConfigureAtmosphereLightIndices = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting", meta = (ClampMin = "0.0"))
	float SunMaximumIntensity = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting", meta = (ClampMin = "0.0"))
	float MoonMaximumIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	FLinearColor DefaultSunColor = FLinearColor(1.0f, 0.86f, 0.66f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	FLinearColor DefaultMoonColor = FLinearColor(0.52f, 0.67f, 1.0f);

	/** Optional multiplier curve evaluated using elevation in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	TObjectPtr<UCurveFloat> SunIntensityByElevation = nullptr;

	/** Optional multiplier curve evaluated using elevation in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	TObjectPtr<UCurveFloat> MoonIntensityByElevation = nullptr;

	/** Optional color curve evaluated using normalized day fraction from 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	TObjectPtr<UCurveLinearColor> SunColorByDayFraction = nullptr;

	/** Optional color curve evaluated using normalized day fraction from 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	TObjectPtr<UCurveLinearColor> MoonColorByDayFraction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions", meta = (ClampMin = "-18.0", ClampMax = "10.0"))
	float SunriseElevationDegrees = -0.833f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky Light")
	bool bRecaptureSkyLight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky Light", meta = (ClampMin = "1.0"))
	float SkyLightRecaptureIntervalSeconds = 30.0f;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherMoonVisualSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	bool bEnabled = true;

	/** Optional profile mesh. If unset, the mesh assigned directly on the controller component is retained. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	TObjectPtr<UStaticMesh> MoonMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon", meta = (ClampMin = "1000.0", UIMin = "10000.0"))
	double OrbitRadius = 500000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon", meta = (ClampMin = "0.0001"))
	double UniformScale = 50.0;

	/** Center the moon on the active player camera to remove open-world parallax. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	bool bCenterOrbitOnPlayerCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	bool bFaceOrbitCenter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	FRotator FacingRotationOffset = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherSkyboxLayerSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox")
	TObjectPtr<UTextureCube> Cubemap = nullptr;

	/** Hue rotation expressed as a 0-1 turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float HueShift = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float Saturation = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox", meta = (ClampMin = "0.0", UIMax = "10.0"))
	float Luminosity = 1.0f;

	/** Optional luminosity multiplier evaluated using normalized day fraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox")
	TObjectPtr<UCurveFloat> LuminosityByDayFraction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox", meta = (ClampMin = "0.0"))
	float MipLevel = 0.0f;
};

USTRUCT(BlueprintType)
struct WEATHERENVIRONMENTSYSTEM_API FWeatherSkyboxSettings
{
	GENERATED_BODY()

	FWeatherSkyboxSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox")
	bool bEnabled = true;

	/** Recommended Opaque/Unlit/Is Sky surface material rendered behind volumetric clouds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Dome", meta = (EditCondition = "!bUseLegacyPostProcessSky", EditConditionHides))
	TObjectPtr<UMaterialInterface> SkyDomeMaterial = nullptr;

	/** Radius in centimetres. The supplied Engine sphere has a 50 cm local radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Dome", meta = (ClampMin = "10000.0", UIMin = "100000.0", EditCondition = "!bUseLegacyPostProcessSky", EditConditionHides))
	double SkyDomeRadius = 10000000.0;

	/** Follow the player camera so the sky never develops open-world parallax. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Dome", meta = (EditCondition = "!bUseLegacyPostProcessSky", EditConditionHides))
	bool bCenterSkyDomeOnPlayerCamera = true;

	/** Scales SkyAtmosphereViewLuminance before the night layers are added. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Dome", meta = (ClampMin = "0.0", EditCondition = "!bUseLegacyPostProcessSky", EditConditionHides))
	float SkyAtmosphereLuminanceMultiplier = 1.0f;

	/** Temporary fallback only. It cannot preserve volumetric clouds at night. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Legacy Post Process")
	bool bUseLegacyPostProcessSky = false;

	/** Legacy post-process material retained for migration and fallback testing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Legacy Post Process", meta = (EditCondition = "bUseLegacyPostProcessSky", EditConditionHides))
	TObjectPtr<UMaterialInterface> PostProcessMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox", meta = (EditFixedSize))
	TArray<FWeatherSkyboxLayerSettings> Layers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Gradient")
	FLinearColor GradientColor = FLinearColor(0.15f, 0.28f, 0.55f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Gradient")
	float GradientScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Gradient", meta = (ClampMin = "0.001"))
	float GradientPower = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Gradient")
	float GradientIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Gradient", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float HorizonOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Tint")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Tint", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TintBlend = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox", meta = (ClampMin = "0.0"))
	float MaximumBrightness = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox", meta = (UIMin = "-180.0", UIMax = "180.0"))
	float RotationDegrees = 0.0f;

	/**
	 * Raw reversed-Z far-plane tolerance. Unlike a linear depth threshold this remains stable
	 * in viewport corners at very wide fields of view.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Legacy Post Process|Depth", meta = (ClampMin = "0.0", ClampMax = "0.01", EditCondition = "bUseLegacyPostProcessSky", EditConditionHides))
	float FarPlaneDeviceZEpsilon = 0.000001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Legacy Post Process|Depth", meta = (ClampMin = "0.0", ClampMax = "0.01", EditCondition = "bUseLegacyPostProcessSky", EditConditionHides))
	float DepthMaskFeather = 0.00001f;

	/** Optional fallback for finite sky domes. Set to zero to use only the raw far-plane test. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Legacy Post Process|Depth", meta = (ClampMin = "0.0", EditCondition = "bUseLegacyPostProcessSky", EditConditionHides))
	float LinearDepthFallbackDistance = 100000000.0f;

	/** Strength of the view-luminance atmospheric blend. Zero disables only this secondary blend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Atmosphere", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AtmosphereFadeStrength = 1.0f;

	/** Optional global fade gate evaluated using sun elevation in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Atmosphere")
	TObjectPtr<UCurveFloat> AtmosphereFadeBySunElevation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Legacy Post Process|Atmosphere", meta = (ClampMin = "0.001", EditCondition = "bUseLegacyPostProcessSky", EditConditionHides))
	float AtmosphereFadePower = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Legacy Post Process|Atmosphere", meta = (ClampMin = "0.0", EditCondition = "bUseLegacyPostProcessSky", EditConditionHides))
	float AtmosphereFadeMinimumLuminance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Legacy Post Process|Atmosphere", meta = (ClampMin = "0.0001", EditCondition = "bUseLegacyPostProcessSky", EditConditionHides))
	float AtmosphereFadeMaximumLuminance = 1.0f;

	/** At or below this solar elevation, the custom night sky is fully visible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Day Night", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float NightSkyFullyVisibleSunElevationDegrees = -6.0f;

	/** At or above this solar elevation, the custom night sky is fully hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Day Night", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float NightSkyHiddenSunElevationDegrees = 0.0f;

	/** Zero disables direction gating; one uses the complete direction-derived day/night fade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Day Night", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DirectionalDaylightFadeStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Legacy Post Process", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bUseLegacyPostProcessSky", EditConditionHides))
	float BlendWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skybox|Legacy Post Process", meta = (EditCondition = "bUseLegacyPostProcessSky", EditConditionHides))
	float PostProcessPriority = 100.0f;
};

UCLASS(BlueprintType)
class WEATHERENVIRONMENTSYSTEM_API UWeatherEnvironmentProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UWeatherEnvironmentProfile();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	FWeatherClockSettings Clock;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	FWeatherAstronomySettings Astronomy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	FWeatherMoonVisualSettings MoonVisual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	FWeatherSkyboxSettings Skybox;
};
