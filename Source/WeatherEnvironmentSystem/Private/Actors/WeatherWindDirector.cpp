// Copyright James Joslin. All Rights Reserved.

#include "Actors/WeatherWindDirector.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"

AWeatherWindDirector::AWeatherWindDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RouteSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RouteSpline"));
	RouteSpline->SetupAttachment(SceneRoot);
	RouteSpline->SetAbsolute(true, true, true);
	RouteSpline->SetMobility(EComponentMobility::Movable);
	RouteSpline->bDrawDebug = true;
}

void AWeatherWindDirector::BeginPlay()
{
	Super::BeginPlay();
	RebuildRouteSpline();
	if (Mode == EWeatherWindDirectorMode::SplineRoute && bStartRouteOnBeginPlay)
	{
		StartRoute();
	}
}

void AWeatherWindDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Mode != EWeatherWindDirectorMode::SplineRoute || !RouteState.bIsRunning)
	{
		return;
	}

	const FVector PreviousLocation = GetActorLocation();
	const FVector NewLocation = FWeatherWindRouteSolver::Advance(
		DeltaSeconds,
		RoutePoints,
		RouteBehavior,
		RouteState,
		PreviousLocation);
	SetActorLocation(NewLocation);

	const FVector Movement = NewLocation - PreviousLocation;
	if (bOrientActorAlongRoute && !Movement.IsNearlyZero())
	{
		SetActorRotation(Movement.GetSafeNormal().Rotation());
	}
}

void AWeatherWindDirector::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildRouteSpline();
}

void AWeatherWindDirector::SetDirectorRoute(
	const TArray<FWeatherWindRoutePoint>& NewRoutePoints,
	const EWeatherWindRouteBehavior NewBehavior)
{
	StopRoute();
	RoutePoints = NewRoutePoints;
	RouteBehavior = NewBehavior;
	Mode = EWeatherWindDirectorMode::SplineRoute;
	RebuildRouteSpline();
}

bool AWeatherWindDirector::StartRoute()
{
	if (Mode != EWeatherWindDirectorMode::SplineRoute)
	{
		return false;
	}

	return FWeatherWindRouteSolver::Start(GetActorLocation(), RoutePoints, RouteState);
}

void AWeatherWindDirector::StopRoute()
{
	FWeatherWindRouteSolver::Stop(RouteState);
}

void AWeatherWindDirector::RebuildRouteSpline()
{
	if (!RouteSpline)
	{
		return;
	}

	RouteSpline->ClearSplinePoints(false);
	for (int32 PointIndex = 0; PointIndex < RoutePoints.Num(); ++PointIndex)
	{
		const FWeatherWindRoutePoint& Point = RoutePoints[PointIndex];
		RouteSpline->AddSplinePoint(Point.Location, ESplineCoordinateSpace::World, false);
		// Movement uses the same eased point-to-point segments, so the displayed spline must match it.
		RouteSpline->SetSplinePointType(PointIndex, ESplinePointType::Linear, false);
	}
	RouteSpline->SetClosedLoop(
		RouteBehavior == EWeatherWindRouteBehavior::Loop && RoutePoints.Num() > 1,
		false);
	RouteSpline->UpdateSpline();
}
