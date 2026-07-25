// Copyright James Joslin. All Rights Reserved.

#include "WeatherEnvironmentSystemEditorModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/TextureCube.h"
#include "HAL/IConsoleManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionSkyAtmosphereViewLuminance.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeatherEnvironmentEditor, Log, All);

namespace WeatherSkyboxMaterial
{
	constexpr TCHAR SourceMaterialPath[] =
		TEXT("/Game/Level_Building/Stars/PostProcess_Stars_Mat2.PostProcess_Stars_Mat2");
	constexpr TCHAR DestinationPackagePath[] =
		TEXT("/WeatherEnvironmentSystem/Materials/M_WeatherSkyboxPostProcess");
	constexpr TCHAR DestinationAssetName[] = TEXT("M_WeatherSkyboxPostProcess");
	constexpr TCHAR SkyDomePackagePath[] =
		TEXT("/WeatherEnvironmentSystem/Materials/M_WeatherSkyDome");
	constexpr TCHAR SkyDomeAssetName[] = TEXT("M_WeatherSkyDome");
	constexpr TCHAR CustomExpressionCode[] =
		TEXT(
			"return WeatherEvaluateSkybox(\n"
			"    Parameters,\n"
			"    SceneColor,\n"
			"    LinearSceneDepth,\n"
			"    AtmosphereLuminance,\n"
			"    WeatherSkybox1, WeatherSkybox1Sampler,\n"
			"    WeatherSkybox2, WeatherSkybox2Sampler,\n"
			"    WeatherSkybox3, WeatherSkybox3Sampler,\n"
			"    WeatherSkybox4, WeatherSkybox4Sampler,\n"
			"    HueShifts,\n"
			"    Saturations,\n"
			"    Luminosities,\n"
			"    MipLevels,\n"
			"    GradientParameters,\n"
			"    GradientColor,\n"
			"    Tint,\n"
			"    TintBlend,\n"
			"    MaximumBrightness,\n"
			"    SkyRotationDegrees,\n"
			"    DepthParameters,\n"
			"    AtmosphereParameters,\n"
			"    DayNightParameters);");
	constexpr TCHAR SkyDomeCustomExpressionCode[] =
		TEXT(
			"return WeatherEvaluateSkyDome(\n"
			"    Parameters,\n"
			"    CameraVector,\n"
			"    AtmosphereLuminance,\n"
			"    WeatherSkybox1, WeatherSkybox1Sampler,\n"
			"    WeatherSkybox2, WeatherSkybox2Sampler,\n"
			"    WeatherSkybox3, WeatherSkybox3Sampler,\n"
			"    WeatherSkybox4, WeatherSkybox4Sampler,\n"
			"    HueShifts,\n"
			"    Saturations,\n"
			"    Luminosities,\n"
			"    MipLevels,\n"
			"    GradientParameters,\n"
			"    GradientColor,\n"
			"    Tint,\n"
			"    DayNightParameters,\n"
			"    TintBlend,\n"
			"    MaximumBrightness,\n"
			"    SkyRotationDegrees);");

	template <typename ExpressionType>
	ExpressionType* CreateExpression(
		UMaterial* Material,
		const int32 PositionX,
		const int32 PositionY)
	{
		ExpressionType* Expression = Cast<ExpressionType>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				ExpressionType::StaticClass(),
				PositionX,
				PositionY));
		check(Expression);
		return Expression;
	}

	void AddCustomInput(
		UMaterialExpressionCustom* Custom,
		const FName InputName,
		UMaterialExpression* Expression,
		const int32 OutputIndex = 0)
	{
		FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
		Input.InputName = InputName;
		Input.Input.Connect(OutputIndex, Expression);
	}

	UMaterialExpressionVectorParameter* AddVectorParameter(
		UMaterial* Material,
		UMaterialExpressionCustom* Custom,
		const FName ParameterName,
		const FName InputName,
		const FLinearColor& DefaultValue,
		const int32 PositionX,
		const int32 PositionY)
	{
		UMaterialExpressionVectorParameter* Parameter =
			CreateExpression<UMaterialExpressionVectorParameter>(Material, PositionX, PositionY);
		Parameter->ParameterName = ParameterName;
		Parameter->DefaultValue = DefaultValue;
		// Vector Parameter output 0 is RGB in UE 5.7. Output 5 is RGBA and is
		// required because the fourth sky layer and several packed settings use A/W.
		AddCustomInput(Custom, InputName, Parameter, 5);
		return Parameter;
	}

	UMaterialExpressionScalarParameter* AddScalarParameter(
		UMaterial* Material,
		UMaterialExpressionCustom* Custom,
		const FName ParameterName,
		const FName InputName,
		const float DefaultValue,
		const int32 PositionX,
		const int32 PositionY)
	{
		UMaterialExpressionScalarParameter* Parameter =
			CreateExpression<UMaterialExpressionScalarParameter>(Material, PositionX, PositionY);
		Parameter->ParameterName = ParameterName;
		Parameter->DefaultValue = DefaultValue;
		AddCustomInput(Custom, InputName, Parameter);
		return Parameter;
	}

	bool RepairVectorCustomInputs(UMaterial* Material)
	{
		bool bChanged = false;
		TArray<UMaterialExpressionCustom*> WeatherCustomExpressions;
		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression);
			if (Custom && Custom->IncludeFilePaths.Contains(
				TEXT("/WeatherEnvironmentSystem/Private/WeatherSkyboxCommon.ush")))
			{
				WeatherCustomExpressions.Add(Custom);
			}
		}

		// Adding a parameter also adds a material expression, so perform repairs
		// after collecting the Custom nodes to avoid invalidating the expression array.
		for (UMaterialExpressionCustom* Custom : WeatherCustomExpressions)
		{
			if (Custom->Code != CustomExpressionCode)
			{
				Custom->Code = CustomExpressionCode;
				bChanged = true;
			}

			for (FCustomInput& Input : Custom->Inputs)
			{
				UMaterialExpression* InputExpression = Input.Input.Expression;
				if (Cast<UMaterialExpressionVectorParameter>(InputExpression)
					&& Input.Input.OutputIndex != 5)
				{
					Input.Input.Connect(5, InputExpression);
					bChanged = true;
				}
			}

			const bool bHasDayNightParameters = Custom->Inputs.ContainsByPredicate(
				[](const FCustomInput& Input)
				{
					return Input.InputName == TEXT("DayNightParameters");
				});
			if (!bHasDayNightParameters)
			{
				AddVectorParameter(
					Material,
					Custom,
					TEXT("WeatherDayNightParams"),
					TEXT("DayNightParameters"),
					FLinearColor(-6.0f, 0.0f, 1.0f, 0.0f),
					-900,
					1780);
				bChanged = true;
			}
		}
		return bChanged;
	}

	bool SaveMaterial(UMaterial* Material)
	{
		const FString PackageName = Material->GetOutermost()->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			PackageName,
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.Error = GError;
		return UPackage::SavePackage(Material->GetOutermost(), Material, *Filename, SaveArgs);
	}
}

void FWeatherEnvironmentSystemEditorModule::StartupModule()
{
	GenerateSkyboxMaterialCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Weather.GenerateSkyboxMaterial"),
		TEXT("Create the Weather four-cubemap master material, or repair/recompile it when it already exists."),
		FConsoleCommandDelegate::CreateRaw(this, &FWeatherEnvironmentSystemEditorModule::GenerateSkyboxMaterial),
		ECVF_Default);

	GenerateSkyDomeMaterialCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Weather.GenerateSkyDomeMaterial"),
		TEXT("Create or repair the preferred Opaque/Unlit/Is Sky four-cubemap dome material."),
		FConsoleCommandDelegate::CreateRaw(this, &FWeatherEnvironmentSystemEditorModule::GenerateSkyDomeMaterial),
		ECVF_Default);
}

void FWeatherEnvironmentSystemEditorModule::ShutdownModule()
{
	if (GenerateSkyboxMaterialCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(GenerateSkyboxMaterialCommand);
		GenerateSkyboxMaterialCommand = nullptr;
	}

	if (GenerateSkyDomeMaterialCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(GenerateSkyDomeMaterialCommand);
		GenerateSkyDomeMaterialCommand = nullptr;
	}
}

void FWeatherEnvironmentSystemEditorModule::GenerateSkyboxMaterial()
{
	using namespace WeatherSkyboxMaterial;

	const FString DestinationObjectPath = FString::Printf(
		TEXT("%s.%s"),
		DestinationPackagePath,
		DestinationAssetName);
	if (UMaterial* ExistingMaterial = LoadObject<UMaterial>(nullptr, *DestinationObjectPath))
	{
		ExistingMaterial->Modify();
		const bool bRepaired = RepairVectorCustomInputs(ExistingMaterial);
		ExistingMaterial->PostEditChange();
		ExistingMaterial->MarkPackageDirty();
		UMaterialEditingLibrary::RecompileMaterial(ExistingMaterial);
		if (!SaveMaterial(ExistingMaterial))
		{
			UE_LOG(
				LogWeatherEnvironmentEditor,
				Error,
				TEXT("Failed to save repaired material %s."),
				*DestinationObjectPath);
			return;
		}
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Display,
			TEXT("%s existing material %s and saved it. Delete the asset first only to regenerate its complete graph."),
			bRepaired ? TEXT("Repaired and recompiled") : TEXT("Validated and recompiled"),
			*DestinationObjectPath);
		return;
	}

	UMaterial* SourceMaterial = LoadObject<UMaterial>(nullptr, SourceMaterialPath);
	TArray<UTextureCube*> SourceCubemaps;
	if (SourceMaterial)
	{
		TArray<TPair<FString, UTextureCube*>> NamedCubemaps;
		for (UMaterialExpression* Expression : SourceMaterial->GetExpressions())
		{
			const UMaterialExpressionTextureSampleParameterCube* CubeParameter =
				Cast<UMaterialExpressionTextureSampleParameterCube>(Expression);
			if (CubeParameter)
			{
				if (UTextureCube* Cube = Cast<UTextureCube>(CubeParameter->Texture))
				{
					NamedCubemaps.Emplace(CubeParameter->ParameterName.ToString(), Cube);
				}
			}
		}

		NamedCubemaps.Sort([](
			const TPair<FString, UTextureCube*>& Left,
			const TPair<FString, UTextureCube*>& Right)
		{
			return Left.Key < Right.Key;
		});
		for (const TPair<FString, UTextureCube*>& Entry : NamedCubemaps)
		{
			SourceCubemaps.AddUnique(Entry.Value);
		}
	}

	UTextureCube* DefaultCube = LoadObject<UTextureCube>(
		nullptr,
		TEXT("/Engine/EngineResources/DefaultTextureCube.DefaultTextureCube"));
	if (!DefaultCube)
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Could not load Unreal's default cubemap."));
		return;
	}

	UPackage* Package = CreatePackage(DestinationPackagePath);
	if (!Package)
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Could not create skybox material package."));
		return;
	}

	UMaterial* Material = NewObject<UMaterial>(
		Package,
		DestinationAssetName,
		RF_Public | RF_Standalone | RF_Transactional);

	Material->Modify();
	Material->MaterialDomain = MD_PostProcess;
	Material->BlendableLocation = BL_SceneColorAfterDOF;
	Material->BlendableOutputAlpha = false;
	Material->BlendablePriority = 0;

	UMaterialExpressionCustom* Custom =
		CreateExpression<UMaterialExpressionCustom>(Material, 600, 0);
	Custom->Description = TEXT("Weather four-cubemap sky: FOV-safe ray and reversed-Z sky mask");
	Custom->OutputType = CMOT_Float4;
	Custom->IncludeFilePaths.Add(TEXT("/WeatherEnvironmentSystem/Private/WeatherSkyboxCommon.ush"));
	Custom->Code = CustomExpressionCode;

	UMaterialExpressionSceneTexture* SceneColor =
		CreateExpression<UMaterialExpressionSceneTexture>(Material, -1400, -500);
	SceneColor->SceneTextureId = PPI_PostProcessInput0;
	AddCustomInput(Custom, TEXT("SceneColor"), SceneColor);

	UMaterialExpressionSceneTexture* SceneDepth =
		CreateExpression<UMaterialExpressionSceneTexture>(Material, -1400, -350);
	SceneDepth->SceneTextureId = PPI_SceneDepth;
	AddCustomInput(Custom, TEXT("LinearSceneDepth"), SceneDepth);

	UMaterialExpressionSkyAtmosphereViewLuminance* Atmosphere =
		CreateExpression<UMaterialExpressionSkyAtmosphereViewLuminance>(Material, -1400, -200);
	AddCustomInput(Custom, TEXT("AtmosphereLuminance"), Atmosphere);

	for (int32 LayerIndex = 0; LayerIndex < 4; ++LayerIndex)
	{
		UMaterialExpressionTextureObjectParameter* TextureParameter =
			CreateExpression<UMaterialExpressionTextureObjectParameter>(
				Material,
				-1400,
				LayerIndex * 160);
		TextureParameter->ParameterName =
			FName(*FString::Printf(TEXT("WeatherSkybox%d"), LayerIndex + 1));
		TextureParameter->Texture = SourceCubemaps.IsValidIndex(LayerIndex)
			? SourceCubemaps[LayerIndex]
			: DefaultCube;
		TextureParameter->SamplerType =
			UMaterialExpressionTextureBase::GetSamplerTypeForTexture(TextureParameter->Texture);
		AddCustomInput(
			Custom,
			FName(*FString::Printf(TEXT("WeatherSkybox%d"), LayerIndex + 1)),
			TextureParameter);
	}

	int32 ParameterY = 700;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherHueShifts"), TEXT("HueShifts"),
		FLinearColor::Transparent, -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherSaturations"), TEXT("Saturations"),
		FLinearColor::White, -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherLuminosities"), TEXT("Luminosities"),
		FLinearColor::White, -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherMipLevels"), TEXT("MipLevels"),
		FLinearColor::Transparent, -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherGradientParams"), TEXT("GradientParameters"),
		FLinearColor(1.0f, 1.0f, 0.0f, 0.0f), -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherGradientColor"), TEXT("GradientColor"),
		FLinearColor(0.15f, 0.28f, 0.55f, 1.0f), -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherTint"), TEXT("Tint"),
		FLinearColor::White, -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherDepthParams"), TEXT("DepthParameters"),
		FLinearColor(0.000001f, 0.00001f, 100000000.0f, 0.0f), -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherAtmosphereParams"), TEXT("AtmosphereParameters"),
		FLinearColor(1.0f, 6.0f, 0.0f, 1.0f), -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherDayNightParams"), TEXT("DayNightParameters"),
		FLinearColor(-6.0f, 0.0f, 1.0f, 0.0f), -900, ParameterY);

	ParameterY = 700;
	AddScalarParameter(
		Material, Custom, TEXT("WeatherTintBlend"), TEXT("TintBlend"),
		0.0f, -500, ParameterY);
	ParameterY += 120;
	AddScalarParameter(
		Material, Custom, TEXT("WeatherMaxBrightness"), TEXT("MaximumBrightness"),
		4.0f, -500, ParameterY);
	ParameterY += 120;
	AddScalarParameter(
		Material, Custom, TEXT("WeatherSkyRotationDegrees"), TEXT("SkyRotationDegrees"),
		0.0f, -500, ParameterY);

	if (!UMaterialEditingLibrary::ConnectMaterialProperty(
		Custom,
		FString(),
		MP_EmissiveColor))
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Could not connect the sky custom expression to Emissive Color."));
		return;
	}

	UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	FAssetRegistryModule::AssetCreated(Material);

	UMaterialEditingLibrary::RecompileMaterial(Material);

	if (!SaveMaterial(Material))
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Failed to save generated material %s."), DestinationPackagePath);
		return;
	}

	UE_LOG(
		LogWeatherEnvironmentEditor,
		Display,
		TEXT("Generated %s using %d cubemap defaults copied from %s."),
		DestinationPackagePath,
		SourceCubemaps.Num(),
		SourceMaterial ? SourceMaterialPath : TEXT("the engine fallback"));
}

void FWeatherEnvironmentSystemEditorModule::GenerateSkyDomeMaterial()
{
	using namespace WeatherSkyboxMaterial;

	const FString DestinationObjectPath = FString::Printf(
		TEXT("%s.%s"),
		SkyDomePackagePath,
		SkyDomeAssetName);
	if (UMaterial* ExistingMaterial = LoadObject<UMaterial>(nullptr, *DestinationObjectPath))
	{
		ExistingMaterial->Modify();
		ExistingMaterial->MaterialDomain = MD_Surface;
		ExistingMaterial->BlendMode = BLEND_Opaque;
		ExistingMaterial->SetShadingModel(MSM_Unlit);
		ExistingMaterial->TwoSided = true;
		ExistingMaterial->bIsSky = true;

		for (UMaterialExpression* Expression : ExistingMaterial->GetExpressions())
		{
			UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression);
			if (!Custom || !Custom->IncludeFilePaths.Contains(
				TEXT("/WeatherEnvironmentSystem/Private/WeatherSkyDomeCommon.ush")))
			{
				continue;
			}

			Custom->Code = SkyDomeCustomExpressionCode;
			Custom->OutputType = CMOT_Float3;
			for (FCustomInput& Input : Custom->Inputs)
			{
				UMaterialExpression* InputExpression = Input.Input.Expression;
				if (Cast<UMaterialExpressionVectorParameter>(InputExpression)
					&& Input.Input.OutputIndex != 5)
				{
					Input.Input.Connect(5, InputExpression);
				}
			}
		}

		ExistingMaterial->PostEditChange();
		ExistingMaterial->MarkPackageDirty();
		UMaterialEditingLibrary::RecompileMaterial(ExistingMaterial);
		if (!SaveMaterial(ExistingMaterial))
		{
			UE_LOG(
				LogWeatherEnvironmentEditor,
				Error,
				TEXT("Failed to save sky-dome material %s."),
				*DestinationObjectPath);
			return;
		}

		UE_LOG(
			LogWeatherEnvironmentEditor,
			Display,
			TEXT("Validated, repaired, and saved existing sky-dome material %s."),
			*DestinationObjectPath);
		return;
	}

	UMaterial* SourceMaterial = LoadObject<UMaterial>(nullptr, SourceMaterialPath);
	TArray<UTextureCube*> SourceCubemaps;
	if (SourceMaterial)
	{
		TArray<TPair<FString, UTextureCube*>> NamedCubemaps;
		for (UMaterialExpression* Expression : SourceMaterial->GetExpressions())
		{
			const UMaterialExpressionTextureSampleParameterCube* CubeParameter =
				Cast<UMaterialExpressionTextureSampleParameterCube>(Expression);
			if (CubeParameter)
			{
				if (UTextureCube* Cube = Cast<UTextureCube>(CubeParameter->Texture))
				{
					NamedCubemaps.Emplace(CubeParameter->ParameterName.ToString(), Cube);
				}
			}
		}

		NamedCubemaps.Sort([](
			const TPair<FString, UTextureCube*>& Left,
			const TPair<FString, UTextureCube*>& Right)
		{
			return Left.Key < Right.Key;
		});
		for (const TPair<FString, UTextureCube*>& Entry : NamedCubemaps)
		{
			SourceCubemaps.AddUnique(Entry.Value);
		}
	}

	UTextureCube* DefaultCube = LoadObject<UTextureCube>(
		nullptr,
		TEXT("/Engine/EngineResources/DefaultTextureCube.DefaultTextureCube"));
	if (!DefaultCube)
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Could not load Unreal's default cubemap."));
		return;
	}

	UPackage* Package = CreatePackage(SkyDomePackagePath);
	if (!Package)
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Could not create sky-dome material package."));
		return;
	}

	UMaterial* Material = NewObject<UMaterial>(
		Package,
		SkyDomeAssetName,
		RF_Public | RF_Standalone | RF_Transactional);
	Material->Modify();
	Material->MaterialDomain = MD_Surface;
	Material->BlendMode = BLEND_Opaque;
	Material->SetShadingModel(MSM_Unlit);
	Material->TwoSided = true;
	Material->bIsSky = true;

	UMaterialExpressionCustom* Custom =
		CreateExpression<UMaterialExpressionCustom>(Material, 600, 0);
	Custom->Description = TEXT("Weather sky dome: atmosphere plus four graded cubemaps");
	Custom->OutputType = CMOT_Float3;
	Custom->IncludeFilePaths.Add(TEXT("/WeatherEnvironmentSystem/Private/WeatherSkyDomeCommon.ush"));
	Custom->Code = SkyDomeCustomExpressionCode;

	UMaterialExpressionCameraVectorWS* CameraVector =
		CreateExpression<UMaterialExpressionCameraVectorWS>(Material, -1400, -500);
	AddCustomInput(Custom, TEXT("CameraVector"), CameraVector);

	UMaterialExpressionSkyAtmosphereViewLuminance* Atmosphere =
		CreateExpression<UMaterialExpressionSkyAtmosphereViewLuminance>(Material, -1400, -350);
	AddCustomInput(Custom, TEXT("AtmosphereLuminance"), Atmosphere);

	for (int32 LayerIndex = 0; LayerIndex < 4; ++LayerIndex)
	{
		UMaterialExpressionTextureObjectParameter* TextureParameter =
			CreateExpression<UMaterialExpressionTextureObjectParameter>(
				Material,
				-1400,
				LayerIndex * 160);
		TextureParameter->ParameterName =
			FName(*FString::Printf(TEXT("WeatherSkybox%d"), LayerIndex + 1));
		TextureParameter->Texture = SourceCubemaps.IsValidIndex(LayerIndex)
			? SourceCubemaps[LayerIndex]
			: DefaultCube;
		TextureParameter->SamplerType =
			UMaterialExpressionTextureBase::GetSamplerTypeForTexture(TextureParameter->Texture);
		AddCustomInput(
			Custom,
			FName(*FString::Printf(TEXT("WeatherSkybox%d"), LayerIndex + 1)),
			TextureParameter);
	}

	int32 ParameterY = 700;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherHueShifts"), TEXT("HueShifts"),
		FLinearColor::Transparent, -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherSaturations"), TEXT("Saturations"),
		FLinearColor::White, -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherLuminosities"), TEXT("Luminosities"),
		FLinearColor::White, -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherMipLevels"), TEXT("MipLevels"),
		FLinearColor::Transparent, -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherGradientParams"), TEXT("GradientParameters"),
		FLinearColor(1.0f, 1.0f, 0.0f, 0.0f), -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherGradientColor"), TEXT("GradientColor"),
		FLinearColor(0.15f, 0.28f, 0.55f, 1.0f), -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherTint"), TEXT("Tint"),
		FLinearColor::White, -900, ParameterY);
	ParameterY += 120;
	AddVectorParameter(
		Material, Custom, TEXT("WeatherDayNightParams"), TEXT("DayNightParameters"),
		FLinearColor(-6.0f, 0.0f, 1.0f, 1.0f), -900, ParameterY);

	ParameterY = 700;
	AddScalarParameter(
		Material, Custom, TEXT("WeatherTintBlend"), TEXT("TintBlend"),
		0.0f, -500, ParameterY);
	ParameterY += 120;
	AddScalarParameter(
		Material, Custom, TEXT("WeatherMaxBrightness"), TEXT("MaximumBrightness"),
		4.0f, -500, ParameterY);
	ParameterY += 120;
	AddScalarParameter(
		Material, Custom, TEXT("WeatherSkyRotationDegrees"), TEXT("SkyRotationDegrees"),
		0.0f, -500, ParameterY);

	if (!UMaterialEditingLibrary::ConnectMaterialProperty(
		Custom,
		FString(),
		MP_EmissiveColor))
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Could not connect the sky dome to Emissive Color."));
		return;
	}

	UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
	Material->PostEditChange();
	Material->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Material);
	UMaterialEditingLibrary::RecompileMaterial(Material);

	if (!SaveMaterial(Material))
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Failed to save generated sky-dome material %s."), SkyDomePackagePath);
		return;
	}

	UE_LOG(
		LogWeatherEnvironmentEditor,
		Display,
		TEXT("Generated %s using %d cubemap defaults copied from %s."),
		SkyDomePackagePath,
		SourceCubemaps.Num(),
		SourceMaterial ? SourceMaterialPath : TEXT("the engine fallback"));
}

IMPLEMENT_MODULE(FWeatherEnvironmentSystemEditorModule, WeatherEnvironmentSystemEditor)
