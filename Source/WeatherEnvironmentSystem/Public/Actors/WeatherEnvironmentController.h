// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherAstronomy.h"
#include "WeatherEnvironmentProfile.h"
#include "WeatherEnvironmentController.generated.h"

class ADirectionalLight;
class ASkyLight;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class USceneComponent;
class UStaticMeshComponent;
class UWeatherStateSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FWeatherCelestialTransitionSignature,
	FWeatherDateTime,
	DateTime);

UCLASS(BlueprintType)
class WEATHERENVIRONMENTSYSTEM_API AWeatherEnvironmentController : public AActor
{
	GENERATED_BODY()

public:
	AWeatherEnvironmentController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Weather|Environment")
	void RefreshEnvironment();

	UFUNCTION(BlueprintCallable, Category = "Weather|Environment")
	void SetEnvironmentProfile(UWeatherEnvironmentProfile* NewProfile);

	UFUNCTION(BlueprintPure, Category = "Weather|Environment")
	UWeatherStateSubsystem* GetWeatherStateSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "Weather|Skybox")
	UMaterialInstanceDynamic* GetSkyboxMaterialInstance() const { return SkyboxMID; }

	UFUNCTION(BlueprintPure, Category = "Weather|Astronomy")
	FVector GetSunDirection() const { return CurrentSunDirection; }

	UFUNCTION(BlueprintPure, Category = "Weather|Astronomy")
	FVector GetMoonDirection() const { return CurrentMoonDirection; }

	UFUNCTION(BlueprintPure, Category = "Weather|Astronomy")
	float GetSunElevationDegrees() const { return CurrentSunElevationDegrees; }

	UFUNCTION(BlueprintPure, Category = "Weather|Astronomy")
	float GetMoonPhase() const { return CurrentMoonPhase; }

	UPROPERTY(BlueprintAssignable, Category = "Weather|Astronomy")
	FWeatherCelestialTransitionSignature OnSunrise;

	UPROPERTY(BlueprintAssignable, Category = "Weather|Astronomy")
	FWeatherCelestialTransitionSignature OnSunset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Assign a sphere, disc, or stylised moon mesh here, or provide one in the profile. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Components")
	TObjectPtr<UStaticMeshComponent> MoonMeshComponent;

	/** Camera-centred Opaque/Unlit/Is Sky surface rendered behind volumetric clouds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Components")
	TObjectPtr<UStaticMeshComponent> SkyDomeMeshComponent;

	/** Unbound component retained only for the legacy post-process fallback. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Components")
	TObjectPtr<UPostProcessComponent> SkyPostProcessComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Profile")
	TObjectPtr<UWeatherEnvironmentProfile> EnvironmentProfile;

	/** Used when EnvironmentProfile is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Fallback")
	FWeatherClockSettings ClockSettings;

	/** Used when EnvironmentProfile is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Fallback")
	FWeatherAstronomySettings AstronomySettings;

	/** Used when EnvironmentProfile is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Fallback")
	FWeatherMoonVisualSettings MoonVisualSettings;

	/** Used when EnvironmentProfile is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Fallback")
	FWeatherSkyboxSettings SkyboxSettings;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Weather|References")
	TObjectPtr<ADirectionalLight> SunLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Weather|References")
	TObjectPtr<ADirectionalLight> MoonLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Weather|References")
	TObjectPtr<ASkyLight> SkyLight;

	/** Explicit orbit center overrides player-camera centering. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Weather|References")
	TObjectPtr<AActor> MoonOrbitCenter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|References")
	bool bAutoDiscoverLights = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Runtime")
	FVector CurrentSunDirection = FVector::UpVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Runtime")
	FVector CurrentMoonDirection = FVector::DownVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Runtime")
	float CurrentSunElevationDegrees = 90.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Runtime")
	float CurrentMoonElevationDegrees = -90.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Runtime", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CurrentMoonPhase = 0.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	const FWeatherClockSettings& GetClockSettings() const;
	const FWeatherAstronomySettings& GetAstronomySettings() const;
	const FWeatherMoonVisualSettings& GetMoonVisualSettings() const;
	const FWeatherSkyboxSettings& GetSkyboxSettings() const;

	void ResolveWorldReferences();
	void ConfigureMoonMesh();
	void ConfigureSkyDomeMesh();
	void InitializeSkyboxMID();
	void UpdateEnvironment(float DeltaSeconds, bool bForce);
	void UpdateDirectionalLights(const FWeatherCelestialState& CelestialState, double DayFraction);
	void UpdateMoonVisual(const FWeatherCelestialState& CelestialState);
	void UpdateSkyDomeVisual();
	void UpdateSkyboxParameters(double DayFraction);
	void UpdateCelestialTransitionEvents(const FWeatherDateTime& DateTime, double SunElevation);

	static float SmoothRange(float Value, float Minimum, float Maximum);
	static float CalculateMoonPhase(const FDateTime& DateTime);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyboxMID;

	TWeakObjectPtr<UWeatherStateSubsystem> WeatherStateSubsystem;
	bool bControllerRegistered = false;
	bool bHasPreviousSunState = false;
	bool bWasSunAboveTransition = false;
	float SkyLightRecaptureAccumulator = 0.0f;
};
