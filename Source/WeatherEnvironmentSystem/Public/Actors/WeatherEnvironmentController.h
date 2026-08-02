// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherAstronomy.h"
#include "WeatherEnvironmentProfile.h"
#include "WeatherEnvironmentController.generated.h"

class ADirectionalLight;
class ALandscapeProxy;
class ASkyLight;
class AWeatherWindDirector;
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
#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

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

	/** Rebuilds the subsystem grid in game/PIE or the transient preview grid in the editor. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Weather|Grid")
	void RebuildGrid();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Weather|Grid")
	void ClearGrid();

	UFUNCTION(BlueprintPure, Category = "Weather|Grid")
	FWeatherGridInfo GetGridInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Weather|Wind")
	void SetWindDirector(AWeatherWindDirector* NewWindDirector);

	UFUNCTION(BlueprintPure, Category = "Weather|Wind")
	bool GetWindAtLocation(const FVector& WorldLocation, FVector& OutWindVector, float& OutGust) const;

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

	/** Used when EnvironmentProfile is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Fallback")
	FWeatherGridDefinition GridDefinition;

	/** Used when EnvironmentProfile is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Fallback")
	FWeatherWindSettings WindSettings;

	/** Explicit landscape sources. When empty, all landscape proxies in the world are discovered. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Weather|Grid")
	TArray<TObjectPtr<ALandscapeProxy>> LandscapeSources;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Grid")
	bool bRebuildGridOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Grid|Debug")
	FWeatherGridDebugSettings GridDebugSettings;

	/** Result of the most recent automatic or manual grid rebuild. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Weather|Grid|Status")
	bool bLastGridBuildSucceeded = false;

	/** Includes the requested cell count when a rebuild is rejected by the safety cap. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Weather|Grid|Status", meta = (MultiLine = "true"))
	FString LastGridBuildMessage = TEXT("Grid has not been built.");

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Weather|References")
	TObjectPtr<ADirectionalLight> SunLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Weather|References")
	TObjectPtr<ADirectionalLight> MoonLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Weather|References")
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Weather|References")
	TObjectPtr<AWeatherWindDirector> WindDirector;

	/** Explicit orbit center overrides player-camera centering. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Weather|References")
	TObjectPtr<AActor> MoonOrbitCenter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|References")
	bool bAutoDiscoverLights = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|References")
	bool bAutoDiscoverWindDirector = true;

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
	const FWeatherGridDefinition& GetGridDefinition() const;
	const FWeatherWindSettings& GetWindSettings() const;

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
	bool RebuildEditorPreviewGrid();
	void UpdateEditorPreviewWind(float DeltaSeconds, bool bForce);
	const FWeatherGrid* GetGridForDebug() const;
#if ENABLE_DRAW_DEBUG
	void DrawWeatherGridDebug() const;
#endif

	static float SmoothRange(float Value, float Minimum, float Maximum);
	static float CalculateMoonPhase(const FDateTime& DateTime);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyboxMID;

	TWeakObjectPtr<UWeatherStateSubsystem> WeatherStateSubsystem;
	bool bControllerRegistered = false;
	bool bHasPreviousSunState = false;
	bool bWasSunAboveTransition = false;
	float SkyLightRecaptureAccumulator = 0.0f;
	double EditorWindSimulationTimeSeconds = 0.0;
	float EditorWindSimulationAccumulator = 0.0f;
	FWeatherGrid EditorPreviewGrid;
};
