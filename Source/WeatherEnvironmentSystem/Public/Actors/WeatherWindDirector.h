// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherWind.h"
#include "WeatherWindDirector.generated.h"

class USceneComponent;
class USplineComponent;

/**
 * Movable source used to derive every weather cell's base wind direction. In manual mode only
 * the actor transform matters. Route mode moves the same actor through explicit world points.
 */
UCLASS(BlueprintType)
class WEATHERENVIRONMENTSYSTEM_API AWeatherWindDirector : public AActor
{
	GENERATED_BODY()

public:
	AWeatherWindDirector();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Weather|Wind Route")
	void SetDirectorRoute(
		const TArray<FWeatherWindRoutePoint>& NewRoutePoints,
		EWeatherWindRouteBehavior NewBehavior);

	UFUNCTION(BlueprintCallable, Category = "Weather|Wind Route")
	bool StartRoute();

	UFUNCTION(BlueprintCallable, Category = "Weather|Wind Route")
	void StopRoute();

	UFUNCTION(BlueprintPure, Category = "Weather|Wind Route")
	bool IsRouteRunning() const { return RouteState.bIsRunning; }

	UFUNCTION(BlueprintPure, Category = "Weather|Wind Route")
	bool IsRoutePaused() const { return RouteState.bIsPaused; }

	UFUNCTION(BlueprintPure, Category = "Weather|Wind Route")
	int32 GetCurrentRoutePointIndex() const { return RouteState.TargetPointIndex; }

	UFUNCTION(BlueprintPure, Category = "Weather|Wind Route")
	FWeatherWindRouteState GetRouteState() const { return RouteState; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Editor/runtime route visualization. Points are authored in world space and remain fixed as the director moves. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|Components")
	TObjectPtr<USplineComponent> RouteSpline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Wind")
	EWeatherWindDirectorMode Mode = EWeatherWindDirectorMode::Manual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Wind Route")
	TArray<FWeatherWindRoutePoint> RoutePoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Wind Route")
	EWeatherWindRouteBehavior RouteBehavior = EWeatherWindRouteBehavior::Loop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Wind Route")
	bool bStartRouteOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Wind Route")
	bool bOrientActorAlongRoute = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Weather|Wind Route")
	FWeatherWindRouteState RouteState;

protected:
	virtual void BeginPlay() override;

private:
	void RebuildRouteSpline();
};
