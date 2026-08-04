// Copyright James Joslin. All Rights Reserved.

#include "WeatherGridDebugComponentVisualizer.h"

#include "Actors/WeatherEnvironmentController.h"
#include "CanvasTypes.h"
#include "Components/WeatherGridDebugComponent.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "SceneView.h"

void FWeatherGridDebugComponentVisualizer::DrawVisualizationHUD(
	const UActorComponent* Component,
	const FViewport* Viewport,
	const FSceneView* View,
	FCanvas* Canvas)
{
	(void)Viewport;

	const UWeatherGridDebugComponent* DebugComponent =
		Cast<const UWeatherGridDebugComponent>(Component);
	const AWeatherEnvironmentController* Controller = DebugComponent
		? Cast<AWeatherEnvironmentController>(DebugComponent->GetOwner())
		: nullptr;
	if (!Controller || !Controller->GridDebugSettings.bEnabled || !View || !Canvas)
	{
		return;
	}

	const FWeatherGrid* Grid = Controller->GetWeatherGridForDebugVisualization();
	if (!Grid || !Grid->GetInfo().bIsValid)
	{
		return;
	}

	const UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const FVector ViewLocation = View->ViewMatrices.GetViewOrigin();
	const double MaximumDrawDistanceSquared = FMath::Square(
		Controller->GridDebugSettings.DrawDistance);
	const bool bLimitDistance = Controller->GridDebugSettings.DrawDistance > 0.0;
	const FWeatherGridInfo& Info = Grid->GetInfo();
	for (int32 Y = 0; Y < Info.Dimensions.Y; ++Y)
	{
		for (int32 X = 0; X < Info.Dimensions.X; ++X)
		{
			const FWeatherCellCoord Coord(X, Y);
			const FWeatherCellState* State = Grid->FindCell(Coord);
			if (!State)
			{
				continue;
			}

			const FVector2D Offset(
				State->WorldCenter.X - ViewLocation.X,
				State->WorldCenter.Y - ViewLocation.Y);
			if (bLimitDistance && Offset.SizeSquared() > MaximumDrawDistanceSquared)
			{
				continue;
			}

			const FString Label = Controller->BuildWeatherCellDebugLabel(Coord, *State);
			if (Label.IsEmpty())
			{
				continue;
			}

			FVector2D PixelLocation;
			const FVector LabelLocation = State->WorldCenter + FVector(0.0, 0.0, 1000.0);
			if (!View->ScreenToPixel(View->WorldToScreen(LabelLocation), PixelLocation))
			{
				continue;
			}

			if (!FMath::IsWithin(PixelLocation.X, 0.0, static_cast<double>(View->UnscaledViewRect.Width()))
				|| !FMath::IsWithin(PixelLocation.Y, 0.0, static_cast<double>(View->UnscaledViewRect.Height())))
			{
				continue;
			}

			TArray<FString> Lines;
			Label.ParseIntoArrayLines(Lines, false);
			const FLinearColor TextColor =
				Controller->GetWeatherTypeDebugColor(State->WeatherType).ReinterpretAsLinear();
			const float LineHeight = FMath::Max(static_cast<float>(Font->GetMaxCharHeight()), 10.0f);
			for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
			{
				Canvas->DrawShadowedString(
					PixelLocation.X,
					PixelLocation.Y + LineIndex * LineHeight,
					*Lines[LineIndex],
					Font,
					TextColor);
			}
		}
	}
}
