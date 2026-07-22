// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FWeatherEnvironmentSystemModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
