// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

/** Renders per-cell weather labels directly to the editor canvas without requiring AHUD. */
class FWeatherGridDebugComponentVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualizationHUD(
		const UActorComponent* Component,
		const FViewport* Viewport,
		const FSceneView* View,
		FCanvas* Canvas) override;
};
