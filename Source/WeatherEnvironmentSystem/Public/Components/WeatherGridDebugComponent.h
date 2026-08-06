// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeatherGridDebugComponent.generated.h"

/**
 * Editor-only visualization anchor owned by WeatherEnvironmentController.
 * Runtime grid state remains plain data; this component exists solely so the
 * editor module can render HUD-independent per-cell labels through FCanvas.
 */
UCLASS(ClassGroup = (Weather), NotBlueprintable, NotPlaceable)
class WEATHERENVIRONMENTSYSTEM_API UWeatherGridDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeatherGridDebugComponent();
};
