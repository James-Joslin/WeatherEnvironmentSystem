// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class IConsoleObject;

class FWeatherEnvironmentSystemEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void GenerateSkyboxMaterial();
	void GenerateSkyDomeMaterial();

	IConsoleObject* GenerateSkyboxMaterialCommand = nullptr;
	IConsoleObject* GenerateSkyDomeMaterialCommand = nullptr;
};
