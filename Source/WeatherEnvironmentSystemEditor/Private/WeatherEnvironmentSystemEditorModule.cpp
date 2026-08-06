// Copyright James Joslin. All Rights Reserved.

#include "WeatherEnvironmentSystemEditorModule.h"

#include "WeatherEnvironmentProfile.h"
#include "WeatherGridDebugComponentVisualizer.h"

#include "Components/WeatherGridDebugComponent.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "HAL/IConsoleManager.h"
#include "Editor/UnrealEdEngine.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionRotateAboutAxis.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionSkyAtmosphereViewLuminance.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "MaterialShared.h"
#include "Misc/PackageName.h"
#include "Misc/CoreDelegates.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#include "UnrealEdGlobals.h"

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

namespace WeatherWindMaterial
{
	constexpr TCHAR ParameterCollectionPackagePath[] =
		TEXT("/WeatherEnvironmentSystem/Materials/MPC_WeatherEnvironment");
	constexpr TCHAR ParameterCollectionAssetName[] = TEXT("MPC_WeatherEnvironment");
	constexpr TCHAR FieldTexturePackagePath[] =
		TEXT("/WeatherEnvironmentSystem/Materials/T_WeatherWindField");
	constexpr TCHAR FieldTextureAssetName[] = TEXT("T_WeatherWindField");
	constexpr TCHAR FoliageFunctionPackagePath[] =
		TEXT("/WeatherEnvironmentSystem/Materials/MF_WeatherFoliageWind");
	constexpr TCHAR FoliageFunctionAssetName[] = TEXT("MF_WeatherFoliageWind");
	constexpr TCHAR WindSampleFunctionPackagePath[] =
		TEXT("/WeatherEnvironmentSystem/Materials/MF_WeatherWindSample");
	constexpr TCHAR WindSampleFunctionAssetName[] = TEXT("MF_WeatherWindSample");
	constexpr int32 FieldTextureResolution = 64;
	constexpr TCHAR FoliageCustomCode[] =
		TEXT(
			"return WeatherEvaluateFoliageWind(\n"
			"    WindFieldTexture, WindFieldTextureSampler,\n"
			"    WorldPosition,\n"
			"    ObjectPosition,\n"
			"    VertexMask,\n"
			"    TimeSeconds,\n"
			"    FieldOriginSize,\n"
			"    LocalWind,\n"
			"    LocalGust,\n"
			"    SwayParameters,\n"
			"    RustleParameters);" );
	constexpr TCHAR WindSampleCustomCode[] =
		TEXT(
			"const float4 Wind = WeatherSampleWindField(\n"
			"    WindFieldTexture, WindFieldTextureSampler,\n"
			"    ObjectPosition, FieldOriginSize, LocalWind, LocalGust);\n"
			"const float SpeedScale = clamp(Wind.z * max(FoliageMapping.x, 0.0), 0.0, 4.0);\n"
			"// Legacy speed inputs are multiplied by absolute material time. Keep the\n"
			"// authored rate stable so a field update cannot rewrite animation phase.\n"
			"return float4(Wind.xy, SpeedScale, 1.0);" );

	template <typename ExpressionType>
	ExpressionType* CreateFunctionExpression(
		UMaterialFunction* Function,
		const int32 PositionX,
		const int32 PositionY)
	{
		ExpressionType* Expression = Cast<ExpressionType>(
			UMaterialEditingLibrary::CreateMaterialExpressionInFunction(
				Function,
				ExpressionType::StaticClass(),
				PositionX,
				PositionY));
		check(Expression);
		return Expression;
	}

	bool SaveAsset(UObject* Asset)
	{
		if (!Asset)
		{
			return false;
		}

		const FString Filename = FPackageName::LongPackageNameToFilename(
			Asset->GetOutermost()->GetName(),
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.Error = GError;
		return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, SaveArgs);
	}

	UMaterialParameterCollection* CreateOrUpdateParameterCollection()
	{
		const FString ObjectPath = FString::Printf(
			TEXT("%s.%s"),
			ParameterCollectionPackagePath,
			ParameterCollectionAssetName);
		UMaterialParameterCollection* Collection =
			LoadObject<UMaterialParameterCollection>(nullptr, *ObjectPath);
		const bool bIsNew = Collection == nullptr;
		if (!Collection)
		{
			UPackage* Package = CreatePackage(ParameterCollectionPackagePath);
			Collection = NewObject<UMaterialParameterCollection>(
				Package,
				ParameterCollectionAssetName,
				RF_Public | RF_Standalone | RF_Transactional);
		}

		Collection->Modify();
		auto AddVector = [Collection](const FName Name, const FLinearColor DefaultValue)
		{
			const bool bExists = Collection->VectorParameters.ContainsByPredicate(
				[Name](const FCollectionVectorParameter& Parameter)
				{
					return Parameter.ParameterName == Name;
				});
			if (!bExists)
			{
				FCollectionVectorParameter& Parameter = Collection->VectorParameters.AddDefaulted_GetRef();
				Parameter.ParameterName = Name;
				Parameter.DefaultValue = DefaultValue;
			}
		};
		auto AddScalar = [Collection](const FName Name, const float DefaultValue)
		{
			const bool bExists = Collection->ScalarParameters.ContainsByPredicate(
				[Name](const FCollectionScalarParameter& Parameter)
				{
					return Parameter.ParameterName == Name;
				});
			if (!bExists)
			{
				FCollectionScalarParameter& Parameter = Collection->ScalarParameters.AddDefaulted_GetRef();
				Parameter.ParameterName = Name;
				Parameter.DefaultValue = DefaultValue;
			}
		};

		AddVector(TEXT("WeatherFieldOriginSize"), FLinearColor::Transparent);
		AddVector(TEXT("WeatherLocalWind"), FLinearColor(1.0f, 0.0f, 500.0f, 0.25f));
		AddScalar(TEXT("WeatherLocalGust"), 0.0f);
		AddScalar(TEXT("WeatherLocalRain"), 0.0f);
		AddScalar(TEXT("WeatherLocalStorminess"), 0.0f);

		// Compatibility aliases preserve the established foliage graph while moving
		// its authority from /Game/Level_Building/Foliage_EnvironmentSettings to
		// the controller-driven Weather MPC.
		AddScalar(TEXT("Grass Wind Small Size"), 1024.0f);
		AddScalar(TEXT("Grass Wind Large Size"), 2000.0f);
		AddScalar(TEXT("Grass Wind Small Amplification"), -70.0f);
		AddScalar(TEXT("Grass Wind Large Amplification"), -150.0f);
		AddScalar(TEXT("Simple Wind Intensity"), 0.75f);
		AddScalar(TEXT("Simple Wind Speed"), 0.4f);
		AddScalar(TEXT("Wind Sway Gradient"), 0.0f);
		AddScalar(TEXT("Wind Sway Gust Frequency"), 0.2f);
		AddScalar(TEXT("Wind Sway Intensity"), 1.0f);
		AddScalar(TEXT("Wind Sway Offset"), 0.992f);
		AddVector(TEXT("Wind Sway Direction"), FLinearColor(0.0f, 0.947775f, 1.0f, 0.0f));

		// Spatial foliage uses authored base values rather than the compatibility
		// aliases above, which are already scaled by the player-local wind sample.
		// X/Y in WeatherFoliageMapping convert the field's normalized speed and gust;
		// Z/W retain the legacy rotate-about-axis convention.
		AddVector(TEXT("WeatherFoliageMapping"), FLinearColor(4.0f, 0.25f, 0.947775f, 1.0f));
		AddVector(TEXT("WeatherFoliageBase"), FLinearColor(0.75f, 0.4f, 1.0f, 0.2f));
		AddVector(TEXT("WeatherFoliageGustBase"), FLinearColor(-70.0f, -150.0f, 1024.0f, 2000.0f));
		Collection->PostEditChange();
		Collection->MarkPackageDirty();
		if (bIsNew)
		{
			FAssetRegistryModule::AssetCreated(Collection);
		}
		return SaveAsset(Collection) ? Collection : nullptr;
	}

	UTexture2D* CreateOrLoadFieldTexture()
	{
		const FString ObjectPath = FString::Printf(
			TEXT("%s.%s"),
			FieldTexturePackagePath,
			FieldTextureAssetName);
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *ObjectPath))
		{
			return Existing;
		}

		UPackage* Package = CreatePackage(FieldTexturePackagePath);
		UTexture2D* Texture = NewObject<UTexture2D>(
			Package,
			FieldTextureAssetName,
			RF_Public | RF_Standalone | RF_Transactional);
		TArray<FColor> Pixels;
		Pixels.Init(FColor(255, 128, 64, 0), FieldTextureResolution * FieldTextureResolution);
		Texture->Source.Init(
			FieldTextureResolution,
			FieldTextureResolution,
			1,
			1,
			TSF_BGRA8,
			reinterpret_cast<const uint8*>(Pixels.GetData()));
		Texture->SRGB = false;
		Texture->CompressionSettings = TC_VectorDisplacementmap;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;
		Texture->PostEditChange();
		Texture->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Texture);
		return SaveAsset(Texture) ? Texture : nullptr;
	}

	UMaterialExpressionCollectionParameter* AddCollectionParameter(
		UMaterialFunction* Function,
		UMaterialExpressionCustom* Custom,
		UMaterialParameterCollection* Collection,
		const FName ParameterName,
		const FName InputName,
		const int32 PositionY)
	{
		UMaterialExpressionCollectionParameter* Parameter =
			CreateFunctionExpression<UMaterialExpressionCollectionParameter>(Function, -850, PositionY);
		Parameter->Collection = Collection;
		Parameter->ParameterName = ParameterName;
		Parameter->ParameterId = Collection->GetParameterId(ParameterName);
		WeatherSkyboxMaterial::AddCustomInput(Custom, InputName, Parameter);
		return Parameter;
	}

	UMaterialFunction* CreateOrRebuildFoliageFunction(
		UMaterialParameterCollection* Collection,
		UTexture2D* FieldTexture)
	{
		const FString ObjectPath = FString::Printf(
			TEXT("%s.%s"),
			FoliageFunctionPackagePath,
			FoliageFunctionAssetName);
		UMaterialFunction* Function = LoadObject<UMaterialFunction>(nullptr, *ObjectPath);
		const bool bIsNew = Function == nullptr;
		if (!Function)
		{
			UPackage* Package = CreatePackage(FoliageFunctionPackagePath);
			Function = NewObject<UMaterialFunction>(
				Package,
				FoliageFunctionAssetName,
				RF_Public | RF_Standalone | RF_Transactional);
		}

		Function->Modify();
		Function->GetExpressionCollection().Empty();
		Function->Description = TEXT(
			"Complete wind WPO for foliage without an existing wind graph. Samples the shared WeatherFieldTexture and MPC fallbacks, then produces slow trunk/branch sway plus separately masked leaf rustle. Do not add it to another wind deformation. Vertex R controls branch flex and Vertex A controls leaf rustle.");
		Function->UserExposedCaption = TEXT("Weather Foliage Wind");
		Function->bExposeToLibrary = true;
		Function->LibraryCategoriesText.Reset();
		Function->LibraryCategoriesText.Add(FText::FromString(TEXT("Weather")));

		UMaterialExpressionCustom* Custom =
			CreateFunctionExpression<UMaterialExpressionCustom>(Function, 350, 0);
		Custom->Description = TEXT("World-space weather wind field, trunk/branch sway, and leaf rustle");
		Custom->OutputType = CMOT_Float3;
		Custom->IncludeFilePaths.Add(TEXT("/WeatherEnvironmentSystem/Private/WeatherWindFieldCommon.ush"));
		Custom->Code = FoliageCustomCode;

		UMaterialExpressionTextureObjectParameter* TextureParameter =
			CreateFunctionExpression<UMaterialExpressionTextureObjectParameter>(Function, -1200, -500);
		TextureParameter->ParameterName = TEXT("WeatherFieldTexture");
		TextureParameter->Texture = FieldTexture;
		TextureParameter->SamplerType =
			UMaterialExpressionTextureBase::GetSamplerTypeForTexture(FieldTexture);
		WeatherSkyboxMaterial::AddCustomInput(Custom, TEXT("WindFieldTexture"), TextureParameter);

		UMaterialExpressionWorldPosition* WorldPosition =
			CreateFunctionExpression<UMaterialExpressionWorldPosition>(Function, -1200, -350);
		WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
		WeatherSkyboxMaterial::AddCustomInput(Custom, TEXT("WorldPosition"), WorldPosition);
		UMaterialExpressionObjectPositionWS* ObjectPosition =
			CreateFunctionExpression<UMaterialExpressionObjectPositionWS>(Function, -1200, -200);
		WeatherSkyboxMaterial::AddCustomInput(Custom, TEXT("ObjectPosition"), ObjectPosition);
		UMaterialExpressionVertexColor* VertexColor =
			CreateFunctionExpression<UMaterialExpressionVertexColor>(Function, -1200, -50);
		// Vertex Color output 0 is RGB in Unreal, even though the source value also
		// contains alpha. Reassemble RGBA explicitly so the Custom expression receives
		// the float4 required for the separately masked leaf-rustle channel.
		UMaterialExpressionAppendVector* VertexMask =
			CreateFunctionExpression<UMaterialExpressionAppendVector>(Function, -850, -50);
		VertexMask->A.Connect(0, VertexColor);
		VertexMask->B.Connect(4, VertexColor);
		WeatherSkyboxMaterial::AddCustomInput(Custom, TEXT("VertexMask"), VertexMask);
		UMaterialExpressionTime* Time =
			CreateFunctionExpression<UMaterialExpressionTime>(Function, -1200, 100);
		WeatherSkyboxMaterial::AddCustomInput(Custom, TEXT("TimeSeconds"), Time);

		AddCollectionParameter(
			Function, Custom, Collection, TEXT("WeatherFieldOriginSize"), TEXT("FieldOriginSize"), 300);
		AddCollectionParameter(
			Function, Custom, Collection, TEXT("WeatherLocalWind"), TEXT("LocalWind"), 450);
		AddCollectionParameter(
			Function, Custom, Collection, TEXT("WeatherLocalGust"), TEXT("LocalGust"), 600);

		UMaterialExpressionVectorParameter* Sway =
			CreateFunctionExpression<UMaterialExpressionVectorParameter>(Function, -850, 800);
		Sway->ParameterName = TEXT("WeatherFoliageSway");
		Sway->DefaultValue = FLinearColor(120.0f, 45.0f, 0.18f, 500.0f);
		WeatherSkyboxMaterial::AddCustomInput(Custom, TEXT("SwayParameters"), Sway, 5);
		UMaterialExpressionVectorParameter* Rustle =
			CreateFunctionExpression<UMaterialExpressionVectorParameter>(Function, -850, 950);
		Rustle->ParameterName = TEXT("WeatherFoliageRustle");
		Rustle->DefaultValue = FLinearColor(8.0f, 3.5f, 300.0f, 0.2f);
		WeatherSkyboxMaterial::AddCustomInput(Custom, TEXT("RustleParameters"), Rustle, 5);

		UMaterialExpressionFunctionOutput* Output =
			CreateFunctionExpression<UMaterialExpressionFunctionOutput>(Function, 750, 0);
		Output->OutputName = TEXT("World Position Offset");
		Output->Description = TEXT(
			"Complete wind displacement. Connect directly to WPO, adding only unrelated non-wind deformation.");
		Output->SortPriority = 0;
		Output->A.Connect(0, Custom);
		Output->ConditionallyGenerateId(true);
		Output->ValidateName();

		UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(Function);
		Function->UpdateFromFunctionResource();
		Function->PostEditChange();
		Function->MarkPackageDirty();
		if (bIsNew)
		{
			FAssetRegistryModule::AssetCreated(Function);
		}
		return SaveAsset(Function) ? Function : nullptr;
	}

	UMaterialExpressionComponentMask* AddComponentMask(
		UMaterialFunction* Function,
		UMaterialExpression* Source,
		const int32 SourceOutputIndex,
		const bool bR,
		const bool bG,
		const bool bB,
		const bool bA,
		const int32 PositionX,
		const int32 PositionY)
	{
		UMaterialExpressionComponentMask* Mask =
			CreateFunctionExpression<UMaterialExpressionComponentMask>(Function, PositionX, PositionY);
		Mask->Input.Connect(SourceOutputIndex, Source);
		Mask->R = bR;
		Mask->G = bG;
		Mask->B = bB;
		Mask->A = bA;
		return Mask;
	}

	UMaterialFunction* CreateOrRebuildWindSampleFunction(
		UMaterialParameterCollection* Collection,
		UTexture2D* FieldTexture)
	{
		const FString ObjectPath = FString::Printf(
			TEXT("%s.%s"),
			WindSampleFunctionPackagePath,
			WindSampleFunctionAssetName);
		UMaterialFunction* Function = LoadObject<UMaterialFunction>(nullptr, *ObjectPath);
		const bool bIsNew = Function == nullptr;
		if (!Function)
		{
			UPackage* Package = CreatePackage(WindSampleFunctionPackagePath);
			Function = NewObject<UMaterialFunction>(
				Package,
				WindSampleFunctionAssetName,
				RF_Public | RF_Standalone | RF_Transactional);
		}

		Function->Modify();
		Function->GetExpressionCollection().Empty();
		Function->Description = TEXT(
			"Samples the shared spatial wind field once at Object Position WS. Returns direction, displacement scale, and a phase-stable legacy rate scale; it does not generate WPO.");
		Function->UserExposedCaption = TEXT("Weather Wind Sample");
		Function->bExposeToLibrary = true;
		Function->LibraryCategoriesText.Reset();
		Function->LibraryCategoriesText.Add(FText::FromString(TEXT("Weather")));

		UMaterialExpressionCustom* Custom =
			CreateFunctionExpression<UMaterialExpressionCustom>(Function, 150, 0);
		Custom->Description = TEXT("Weather spatial wind sample (no deformation)");
		Custom->OutputType = CMOT_Float4;
		Custom->IncludeFilePaths.Add(TEXT("/WeatherEnvironmentSystem/Private/WeatherWindFieldCommon.ush"));
		Custom->Code = WindSampleCustomCode;

		UMaterialExpressionTextureObjectParameter* TextureParameter =
			CreateFunctionExpression<UMaterialExpressionTextureObjectParameter>(Function, -1100, -450);
		TextureParameter->ParameterName = TEXT("WeatherFieldTexture");
		TextureParameter->Texture = FieldTexture;
		TextureParameter->SamplerType =
			UMaterialExpressionTextureBase::GetSamplerTypeForTexture(FieldTexture);
		WeatherSkyboxMaterial::AddCustomInput(Custom, TEXT("WindFieldTexture"), TextureParameter);

		// Object Position WS is stable for every vertex in one foliage instance. This
		// prevents a large trunk from sampling different directions across its height.
		UMaterialExpressionObjectPositionWS* ObjectPosition =
			CreateFunctionExpression<UMaterialExpressionObjectPositionWS>(Function, -1100, -275);
		WeatherSkyboxMaterial::AddCustomInput(Custom, TEXT("ObjectPosition"), ObjectPosition);

		AddCollectionParameter(
			Function, Custom, Collection, TEXT("WeatherFieldOriginSize"), TEXT("FieldOriginSize"), -100);
		AddCollectionParameter(
			Function, Custom, Collection, TEXT("WeatherLocalWind"), TEXT("LocalWind"), 75);
		AddCollectionParameter(
			Function, Custom, Collection, TEXT("WeatherLocalGust"), TEXT("LocalGust"), 250);
		UMaterialExpressionCollectionParameter* Mapping = AddCollectionParameter(
			Function, Custom, Collection, TEXT("WeatherFoliageMapping"), TEXT("FoliageMapping"), 425);

		UMaterialExpressionComponentMask* Direction = AddComponentMask(
			Function, Custom, 0, true, true, false, false, 450, -150);
		UMaterialExpressionComponentMask* SpeedScale = AddComponentMask(
			Function, Custom, 0, false, false, true, false, 450, 25);
		UMaterialExpressionComponentMask* AnimationScale = AddComponentMask(
			Function, Custom, 0, false, false, false, true, 450, 200);
		(void)Mapping;

		auto AddOutput = [Function](
			UMaterialExpression* Source,
			const FName Name,
			const FString& Description,
			const int32 SortPriority,
			const FGuid& StableId,
			const int32 PositionY)
		{
			UMaterialExpressionFunctionOutput* Output =
				CreateFunctionExpression<UMaterialExpressionFunctionOutput>(Function, 750, PositionY);
			Output->OutputName = Name;
			Output->Description = Description;
			Output->SortPriority = SortPriority;
			Output->Id = StableId;
			Output->A.Connect(0, Source);
			Output->ValidateName();
		};

		AddOutput(
			Direction,
			TEXT("Direction"),
			TEXT("Normalized XY wind direction at this foliage instance."),
			0,
			FGuid(0xE22352D1, 0xB4534194, 0xA99FD29D, 0xE8308165),
			-150);
		AddOutput(
			SpeedScale,
			TEXT("Speed Scale"),
			TEXT("Local speed divided by the foliage reference speed, clamped to 0..4."),
			1,
			FGuid(0xC647A8E7, 0x164B441A, 0x9B595DF4, 0xB6FC7B18),
			25);
		AddOutput(
			AnimationScale,
			TEXT("Animation Scale"),
			TEXT("Phase-stable rate scale. One preserves the established function's authored animation phase."),
			2,
			FGuid(0x41A9A708, 0xF9034F92, 0xBBC719F7, 0xA7FF325C),
			200);

		UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(Function);
		Function->UpdateFromFunctionResource();
		Function->PostEditChange();
		Function->MarkPackageDirty();
		if (bIsNew)
		{
			FAssetRegistryModule::AssetCreated(Function);
		}
		return SaveAsset(Function) ? Function : nullptr;
	}
}

namespace WeatherLegacyFoliageIntegration
{
	constexpr TCHAR SourceCollectionPath[] =
		TEXT("/Game/Level_Building/Foliage_EnvironmentSettings.Foliage_EnvironmentSettings");
	constexpr TCHAR FunctionPaths[][128] =
	{
		TEXT("/Game/Level_Building/MF/Common/MF_GustingWind.MF_GustingWind"),
		TEXT("/Game/Level_Building/MF/Common/MF_SimpleWInd.MF_SimpleWInd"),
		TEXT("/Game/Level_Building/MF/Foliage/MF_FoliageWind_Rustle.MF_FoliageWind_Rustle"),
		TEXT("/Game/Level_Building/MF/Foliage/MF_FoliageWind_Sway.MF_FoliageWind_Sway")
	};
	constexpr TCHAR MaterialPaths[][128] =
	{
		TEXT("/Game/Level_Building/M_Grass_Master.M_Grass_Master"),
		TEXT("/Game/Level_Building/M_Plants_Master.M_Plants_Master"),
		TEXT("/Game/Level_Building/M_Tree_GlobalWind_Master.M_Tree_GlobalWind_Master")
	};
	constexpr TCHAR CompleteWeatherFoliageFunctionPath[] =
		TEXT("/WeatherEnvironmentSystem/Materials/MF_WeatherFoliageWind.MF_WeatherFoliageWind");
	constexpr TCHAR MainWorldWeatherProfilePath[] =
		TEXT("/Game/Level_Building/Weather/DA_MainWorld_WeatherEnvironment.DA_MainWorld_WeatherEnvironment");
	constexpr float SpatialFoliageWindCadence = 1.0f / 30.0f;
	constexpr TCHAR PhaseStableGustOrientationCode[] =
		TEXT(
			"const float2 D = dot(Direction, Direction) > 0.0001\n"
			"    ? normalize(Direction) : float2(1.0, 0.0);\n"
			"const float2 Perpendicular = float2(-D.y, D.x);\n"
			"// MF_GustingWind is scalar in some grass static permutations. An explicit\n"
			"// promotion preserves scalar splatting while making every swizzle valid.\n"
			"const float3 WPO = OriginalWPO + float3(0.0, 0.0, 0.0);\n"
			"return float3(D * WPO.x + Perpendicular * WPO.y, WPO.z);");

	int32 UpgradeMainWorldWindCadence()
	{
		UWeatherEnvironmentProfile* Profile = LoadObject<UWeatherEnvironmentProfile>(
			nullptr,
			MainWorldWeatherProfilePath);
		if (!Profile
			|| (!FMath::IsNearlyEqual(Profile->Wind.FixedUpdateIntervalSeconds, 1.0f)
				&& !FMath::IsNearlyEqual(Profile->Wind.FixedUpdateIntervalSeconds, 0.1f)))
		{
			return 0;
		}

		const float PreviousCadence = Profile->Wind.FixedUpdateIntervalSeconds;
		Profile->Modify();
		Profile->Wind.FixedUpdateIntervalSeconds = SpatialFoliageWindCadence;
		Profile->PostEditChange();
		Profile->MarkPackageDirty();
		if (!WeatherWindMaterial::SaveAsset(Profile))
		{
			UE_LOG(
				LogWeatherEnvironmentEditor,
				Error,
				TEXT("Failed to save the foliage-ready 30 Hz cadence to %s."),
				MainWorldWeatherProfilePath);
			return 0;
		}

		UE_LOG(
			LogWeatherEnvironmentEditor,
			Display,
			TEXT("Updated %s wind cadence from %.3f seconds to %.3f seconds (30 Hz)."),
			MainWorldWeatherProfilePath,
			PreviousCadence,
			SpatialFoliageWindCadence);
		return 1;
	}

	UMaterialExpressionCollectionParameter* FindCollectionParameter(
		UMaterialFunction* Function,
		const FName ParameterName)
	{
		for (UMaterialExpression* Expression : Function->GetExpressions())
		{
			UMaterialExpressionCollectionParameter* Parameter =
				Cast<UMaterialExpressionCollectionParameter>(Expression);
			if (Parameter && Parameter->ParameterName == ParameterName)
			{
				return Parameter;
			}
		}
		return nullptr;
	}

	UMaterialExpressionFunctionOutput* FindFunctionOutput(UMaterialFunction* Function)
	{
		for (UMaterialExpression* Expression : Function->GetExpressions())
		{
			if (UMaterialExpressionFunctionOutput* Output =
				Cast<UMaterialExpressionFunctionOutput>(Expression))
			{
				return Output;
			}
		}
		return nullptr;
	}

	UMaterialExpressionCollectionParameter* CreateCollectionParameter(
		UMaterialFunction* Function,
		UMaterialParameterCollection* Collection,
		const FName ParameterName,
		const int32 PositionX,
		const int32 PositionY)
	{
		UMaterialExpressionCollectionParameter* Parameter =
			WeatherWindMaterial::CreateFunctionExpression<UMaterialExpressionCollectionParameter>(
				Function,
				PositionX,
				PositionY);
		Parameter->Collection = Collection;
		Parameter->ParameterName = ParameterName;
		Parameter->ParameterId = Collection->GetParameterId(ParameterName);
		return Parameter;
	}

	int32 FindFunctionOutputIndex(
		const UMaterialExpressionMaterialFunctionCall* Call,
		const FName OutputName)
	{
		for (int32 Index = 0; Index < Call->FunctionOutputs.Num(); ++Index)
		{
			const FFunctionExpressionOutput& Output = Call->FunctionOutputs[Index];
			if (Output.ExpressionOutput && Output.ExpressionOutput->OutputName == OutputName)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	UMaterialExpressionMaterialFunctionCall* FindSpatialSampleCall(
		UMaterialFunction* Function,
		UMaterialFunction* WindSampleFunction)
	{
		for (UMaterialExpression* Expression : Function->GetExpressions())
		{
			UMaterialExpressionMaterialFunctionCall* Call =
				Cast<UMaterialExpressionMaterialFunctionCall>(Expression);
			if (Call && Call->MaterialFunction == WindSampleFunction)
			{
				return Call;
			}
		}
		return nullptr;
	}

	UMaterialExpressionMaterialFunctionCall* CreateSpatialSampleCall(
		UMaterialFunction* Function,
		UMaterialFunction* WindSampleFunction,
		const int32 PositionX,
		const int32 PositionY)
	{
		UMaterialExpressionMaterialFunctionCall* Call =
			WeatherWindMaterial::CreateFunctionExpression<UMaterialExpressionMaterialFunctionCall>(
				Function,
				PositionX,
				PositionY);
		if (!Call->SetMaterialFunction(WindSampleFunction))
		{
			UE_LOG(
				LogWeatherEnvironmentEditor,
				Error,
				TEXT("Could not assign %s in %s."),
				*WindSampleFunction->GetPathName(),
				*Function->GetPathName());
			return nullptr;
		}
		Call->Desc = TEXT("Weather spatial wind sample (generated)");
		return Call;
	}

	int32 ReplaceConsumers(
		UMaterialFunction* Function,
		UMaterialExpression* Source,
		UMaterialExpression* Replacement,
		const int32 ReplacementOutputIndex)
	{
		if (!Source || !Replacement)
		{
			return 0;
		}

		int32 ReplacedCount = 0;
		for (UMaterialExpression* Expression : Function->GetExpressions())
		{
			for (int32 InputIndex = 0; ; ++InputIndex)
			{
				FExpressionInput* Input = Expression->GetInput(InputIndex);
				if (!Input)
				{
					break;
				}
				if (Input && Input->Expression == Source)
				{
					Input->Connect(ReplacementOutputIndex, Replacement);
					++ReplacedCount;
				}
			}
		}
		return ReplacedCount;
	}

	UMaterialExpressionMultiply* CreateScaleMultiply(
		UMaterialFunction* Function,
		UMaterialExpression* BaseValue,
		UMaterialExpressionMaterialFunctionCall* Sample,
		const int32 SampleOutputIndex,
		const int32 PositionX,
		const int32 PositionY)
	{
		UMaterialExpressionMultiply* Multiply =
			WeatherWindMaterial::CreateFunctionExpression<UMaterialExpressionMultiply>(
				Function,
				PositionX,
				PositionY);
		Multiply->A.Connect(0, BaseValue);
		Multiply->B.Connect(SampleOutputIndex, Sample);
		return Multiply;
	}

	bool ResolveSampleOutputs(
		UMaterialExpressionMaterialFunctionCall* Sample,
		int32& OutDirection,
		int32& OutSpeedScale,
		int32& OutAnimationScale)
	{
		// The sampler asset is regenerated immediately before migration. Refresh an
		// already-authored call so its cached output names and stable GUID links see
		// the new function resource in this same editor process.
		Sample->UpdateFromFunctionResource();
		OutDirection = FindFunctionOutputIndex(Sample, TEXT("Direction"));
		OutSpeedScale = FindFunctionOutputIndex(Sample, TEXT("Speed Scale"));
		OutAnimationScale = FindFunctionOutputIndex(Sample, TEXT("Animation Scale"));
		return OutDirection != INDEX_NONE
			&& OutSpeedScale != INDEX_NONE
			&& OutAnimationScale != INDEX_NONE;
	}

	int32 IntegrateSpatialSway(
		UMaterialFunction* Function,
		UMaterialFunction* WindSampleFunction,
		UMaterialParameterCollection* Collection)
	{
		if (FindSpatialSampleCall(Function, WindSampleFunction))
		{
			return 0;
		}

		UMaterialExpressionCollectionParameter* OldDirection =
			FindCollectionParameter(Function, TEXT("Wind Sway Direction"));
		UMaterialExpressionCollectionParameter* OldIntensity =
			FindCollectionParameter(Function, TEXT("Wind Sway Intensity"));
		UMaterialExpressionCollectionParameter* OldFrequency =
			FindCollectionParameter(Function, TEXT("Wind Sway Gust Frequency"));
		if (!OldDirection || !OldIntensity || !OldFrequency)
		{
			UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Unexpected sway graph in %s."), *Function->GetPathName());
			return 0;
		}

		UMaterialExpressionMaterialFunctionCall* Sample =
			CreateSpatialSampleCall(Function, WindSampleFunction, -3100, -1150);
		int32 DirectionOutput = INDEX_NONE;
		int32 SpeedOutput = INDEX_NONE;
		int32 AnimationOutput = INDEX_NONE;
		if (!Sample || !ResolveSampleOutputs(Sample, DirectionOutput, SpeedOutput, AnimationOutput))
		{
			return 0;
		}

		UMaterialExpressionCollectionParameter* Mapping = CreateCollectionParameter(
			Function, Collection, TEXT("WeatherFoliageMapping"), -2850, -1100);
		UMaterialExpressionCustom* Axis =
			WeatherWindMaterial::CreateFunctionExpression<UMaterialExpressionCustom>(Function, -2450, -1050);
		Axis->Desc = TEXT("Weather spatial legacy sway axis (generated)");
		Axis->Description = TEXT("Convert world wind direction to the existing RotateAboutAxis convention");
		Axis->OutputType = CMOT_Float3;
		Axis->Code = TEXT(
			"const float2 SafeDirection = dot(Direction, Direction) > 0.0001\n"
			"    ? normalize(Direction) : float2(1.0, 0.0);\n"
			"return float3(-SafeDirection.y * Mapping.z, SafeDirection.x * Mapping.z, Mapping.w);");
		WeatherSkyboxMaterial::AddCustomInput(Axis, TEXT("Direction"), Sample, DirectionOutput);
		WeatherSkyboxMaterial::AddCustomInput(Axis, TEXT("Mapping"), Mapping);

		UMaterialExpressionCollectionParameter* Base = CreateCollectionParameter(
			Function, Collection, TEXT("WeatherFoliageBase"), -2850, -850);
		UMaterialExpressionComponentMask* BaseIntensity = WeatherWindMaterial::AddComponentMask(
			Function, Base, 0, false, false, true, false, -2550, -800);
		UMaterialExpressionComponentMask* BaseFrequency = WeatherWindMaterial::AddComponentMask(
			Function, Base, 0, false, false, false, true, -2550, -650);
		UMaterialExpressionMultiply* SpatialIntensity = CreateScaleMultiply(
			Function, BaseIntensity, Sample, SpeedOutput, -2250, -800);
		UMaterialExpressionMultiply* SpatialFrequency = CreateScaleMultiply(
			Function, BaseFrequency, Sample, AnimationOutput, -2250, -650);

		int32 Changes = 0;
		Changes += ReplaceConsumers(Function, OldDirection, Axis, 0);
		Changes += ReplaceConsumers(Function, OldIntensity, SpatialIntensity, 0);
		Changes += ReplaceConsumers(Function, OldFrequency, SpatialFrequency, 0);
		return Changes;
	}

	int32 IntegrateSpatialSimpleParameters(
		UMaterialFunction* Function,
		UMaterialExpressionMaterialFunctionCall* Sample,
		UMaterialParameterCollection* Collection,
		const int32 SpeedOutput,
		const int32 AnimationOutput)
	{
		UMaterialExpressionCollectionParameter* OldIntensity =
			FindCollectionParameter(Function, TEXT("Simple Wind Intensity"));
		UMaterialExpressionCollectionParameter* OldSpeed =
			FindCollectionParameter(Function, TEXT("Simple Wind Speed"));
		if (!OldIntensity || !OldSpeed)
		{
			return 0;
		}

		UMaterialExpressionCollectionParameter* Base = CreateCollectionParameter(
			Function, Collection, TEXT("WeatherFoliageBase"), -2050, -500);
		UMaterialExpressionComponentMask* BaseIntensity = WeatherWindMaterial::AddComponentMask(
			Function, Base, 0, true, false, false, false, -1800, -450);
		UMaterialExpressionComponentMask* BaseSpeed = WeatherWindMaterial::AddComponentMask(
			Function, Base, 0, false, true, false, false, -1800, -300);
		UMaterialExpressionMultiply* SpatialIntensity = CreateScaleMultiply(
			Function, BaseIntensity, Sample, SpeedOutput, -1500, -450);
		UMaterialExpressionMultiply* SpatialSpeed = CreateScaleMultiply(
			Function, BaseSpeed, Sample, AnimationOutput, -1500, -300);

		return ReplaceConsumers(Function, OldIntensity, SpatialIntensity, 0)
			+ ReplaceConsumers(Function, OldSpeed, SpatialSpeed, 0);
	}

	int32 IntegrateSpatialRustle(
		UMaterialFunction* Function,
		UMaterialFunction* WindSampleFunction,
		UMaterialParameterCollection* Collection)
	{
		if (FindSpatialSampleCall(Function, WindSampleFunction))
		{
			return 0;
		}

		UMaterialExpressionMaterialFunctionCall* Sample =
			CreateSpatialSampleCall(Function, WindSampleFunction, -2250, -250);
		int32 DirectionOutput = INDEX_NONE;
		int32 SpeedOutput = INDEX_NONE;
		int32 AnimationOutput = INDEX_NONE;
		if (!Sample || !ResolveSampleOutputs(Sample, DirectionOutput, SpeedOutput, AnimationOutput))
		{
			return 0;
		}
		return IntegrateSpatialSimpleParameters(
			Function, Sample, Collection, SpeedOutput, AnimationOutput);
	}

	int32 IntegrateSpatialSimpleWind(
		UMaterialFunction* Function,
		UMaterialFunction* WindSampleFunction,
		UMaterialParameterCollection* Collection)
	{
		if (FindSpatialSampleCall(Function, WindSampleFunction))
		{
			return 0;
		}

		UMaterialExpressionFunctionOutput* Output = FindFunctionOutput(Function);
		if (!Output || !Output->A.Expression)
		{
			return 0;
		}
		UMaterialExpression* OriginalWpo = Output->A.Expression;
		const int32 OriginalWpoOutputIndex = Output->A.OutputIndex;
		UMaterialExpressionMaterialFunctionCall* Sample =
			CreateSpatialSampleCall(Function, WindSampleFunction, -2200, -650);
		int32 DirectionOutput = INDEX_NONE;
		int32 SpeedOutput = INDEX_NONE;
		int32 AnimationOutput = INDEX_NONE;
		if (!Sample || !ResolveSampleOutputs(Sample, DirectionOutput, SpeedOutput, AnimationOutput))
		{
			return 0;
		}

		int32 Changes = IntegrateSpatialSimpleParameters(
			Function, Sample, Collection, SpeedOutput, AnimationOutput);
		UMaterialExpressionCustom* Orient =
			WeatherWindMaterial::CreateFunctionExpression<UMaterialExpressionCustom>(Function, 20, 450);
		Orient->Desc = TEXT("Weather spatial simple-wind orientation (generated)");
		Orient->Description = TEXT("Rotate SimpleGrassWind's planar result from its +X basis into local weather wind");
		Orient->OutputType = CMOT_Float3;
		Orient->Code = TEXT(
			"const float2 D = dot(Direction, Direction) > 0.0001\n"
			"    ? normalize(Direction) : float2(1.0, 0.0);\n"
			"const float2 Perpendicular = float2(-D.y, D.x);\n"
			"return float3(D * OriginalWPO.x + Perpendicular * OriginalWPO.y, OriginalWPO.z);");
		WeatherSkyboxMaterial::AddCustomInput(
			Orient, TEXT("OriginalWPO"), OriginalWpo, OriginalWpoOutputIndex);
		WeatherSkyboxMaterial::AddCustomInput(Orient, TEXT("Direction"), Sample, DirectionOutput);
		Output->A.Connect(0, Orient);
		return Changes + 1;
	}

	int32 IntegrateSpatialGustingWind(
		UMaterialFunction* Function,
		UMaterialFunction* WindSampleFunction,
		UMaterialParameterCollection* Collection)
	{
		UMaterialExpressionMaterialFunctionCall* Sample =
			FindSpatialSampleCall(Function, WindSampleFunction);
		const bool bNewSample = Sample == nullptr;
		if (bNewSample)
		{
			Sample = CreateSpatialSampleCall(Function, WindSampleFunction, -2200, -550);
		}
		int32 DirectionOutput = INDEX_NONE;
		int32 SpeedOutput = INDEX_NONE;
		int32 AnimationOutput = INDEX_NONE;
		if (!Sample || !ResolveSampleOutputs(Sample, DirectionOutput, SpeedOutput, AnimationOutput))
		{
			return 0;
		}
		(void)AnimationOutput;

		int32 Changes = 0;
		if (bNewSample)
		{
			UMaterialExpressionCollectionParameter* OldSmallAmplification =
				FindCollectionParameter(Function, TEXT("Grass Wind Small Amplification"));
			UMaterialExpressionCollectionParameter* OldLargeAmplification =
				FindCollectionParameter(Function, TEXT("Grass Wind Large Amplification"));
			if (!OldSmallAmplification || !OldLargeAmplification)
			{
				return 0;
			}

			UMaterialExpressionCollectionParameter* Base = CreateCollectionParameter(
				Function, Collection, TEXT("WeatherFoliageGustBase"), -1950, -350);
			UMaterialExpressionComponentMask* BaseSmall = WeatherWindMaterial::AddComponentMask(
				Function, Base, 0, true, false, false, false, -1700, -350);
			UMaterialExpressionComponentMask* BaseLarge = WeatherWindMaterial::AddComponentMask(
				Function, Base, 0, false, true, false, false, -1700, -200);
			UMaterialExpressionMultiply* SpatialSmall = CreateScaleMultiply(
				Function, BaseSmall, Sample, SpeedOutput, -1400, -350);
			UMaterialExpressionMultiply* SpatialLarge = CreateScaleMultiply(
				Function, BaseLarge, Sample, SpeedOutput, -1400, -200);
			Changes += ReplaceConsumers(Function, OldSmallAmplification, SpatialSmall, 0)
				+ ReplaceConsumers(Function, OldLargeAmplification, SpatialLarge, 0);
		}

		// The first spatial migration drove Panner.Speed with the live field
		// direction. Panner evaluates UV + Speed * absolute time, so every direction
		// update rewrote the entire phase and made shrubs jump. Restore the original
		// constant pan rates and rotate only the final planar WPO below.
		TArray<UMaterialExpressionConstant2Vector*> PanDirections;
		for (UMaterialExpression* Expression : Function->GetExpressions())
		{
			UMaterialExpressionConstant2Vector* PanDirection =
				Cast<UMaterialExpressionConstant2Vector>(Expression);
			if (!PanDirection || !FMath::IsNearlyZero(PanDirection->R))
			{
				continue;
			}
			const float Rate = FMath::Abs(PanDirection->G);
			if (!FMath::IsNearlyEqual(Rate, 0.1f) && !FMath::IsNearlyEqual(Rate, 0.2f))
			{
				continue;
			}
			PanDirections.Add(PanDirection);
		}

		for (UMaterialExpressionConstant2Vector* PanDirection : PanDirections)
		{
			const float Rate = FMath::Abs(PanDirection->G);
			for (UMaterialExpression* Expression : Function->GetExpressions())
			{
				UMaterialExpressionMultiply* SpatialPan = Cast<UMaterialExpressionMultiply>(Expression);
				if (!SpatialPan
					|| SpatialPan->A.Expression != Sample
					|| SpatialPan->A.OutputIndex != DirectionOutput)
				{
					continue;
				}

				UMaterialExpressionMultiply* AnimatedRate =
					Cast<UMaterialExpressionMultiply>(SpatialPan->B.Expression);
				if (!AnimatedRate
					|| AnimatedRate->A.Expression != Sample
					|| !FMath::IsNearlyEqual(FMath::Abs(AnimatedRate->ConstB), Rate))
				{
					continue;
				}

				Changes += ReplaceConsumers(Function, SpatialPan, PanDirection, 0);
			}
		}

		UMaterialExpressionCustom* ExistingOrientation = nullptr;
		for (UMaterialExpression* Expression : Function->GetExpressions())
		{
			UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression);
			if (Custom && Custom->Desc == TEXT("Weather spatial gust-wind orientation (generated)"))
			{
				ExistingOrientation = Custom;
				break;
			}
		}

		if (!ExistingOrientation)
		{
			UMaterialExpressionFunctionOutput* Output = FindFunctionOutput(Function);
			if (!Output || !Output->A.Expression)
			{
				return Changes;
			}
			UMaterialExpression* OriginalWpo = Output->A.Expression;
			const int32 OriginalWpoOutputIndex = Output->A.OutputIndex;
			UMaterialExpressionCustom* Orient =
				WeatherWindMaterial::CreateFunctionExpression<UMaterialExpressionCustom>(Function, 250, 350);
			Orient->Desc = TEXT("Weather spatial gust-wind orientation (generated)");
			Orient->Description = TEXT("Rotate phase-stable gust WPO into the local weather direction");
			Orient->OutputType = CMOT_Float3;
			Orient->Code = PhaseStableGustOrientationCode;
			WeatherSkyboxMaterial::AddCustomInput(
				Orient, TEXT("OriginalWPO"), OriginalWpo, OriginalWpoOutputIndex);
			WeatherSkyboxMaterial::AddCustomInput(Orient, TEXT("Direction"), Sample, DirectionOutput);
			Output->A.Connect(0, Orient);
			++Changes;
		}
		else if (ExistingOrientation->Code != PhaseStableGustOrientationCode)
		{
			ExistingOrientation->Code = PhaseStableGustOrientationCode;
			++Changes;
		}
		return Changes;
	}

	int32 IntegrateSpatialWind(
		UMaterialFunction* Function,
		UMaterialFunction* WindSampleFunction,
		UMaterialParameterCollection* Collection)
	{
		const FString Path = Function->GetPathName();
		if (Path.Contains(TEXT("MF_FoliageWind_Sway")))
		{
			return IntegrateSpatialSway(Function, WindSampleFunction, Collection);
		}
		if (Path.Contains(TEXT("MF_FoliageWind_Rustle")))
		{
			return IntegrateSpatialRustle(Function, WindSampleFunction, Collection);
		}
		if (Path.Contains(TEXT("MF_SimpleWInd")))
		{
			return IntegrateSpatialSimpleWind(Function, WindSampleFunction, Collection);
		}
		if (Path.Contains(TEXT("MF_GustingWind")))
		{
			return IntegrateSpatialGustingWind(Function, WindSampleFunction, Collection);
		}
		return 0;
	}

	int32 RestorePlantsMasterWpo(UMaterial* Material)
	{
		if (!Material || !Material->GetPathName().Contains(TEXT("M_Plants_Master")))
		{
			return 0;
		}

		UMaterialExpressionMaterialFunctionCall* CompleteWeatherCall = nullptr;
		UMaterialExpressionStaticSwitchParameter* OriginalWindSwitch = nullptr;
		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (UMaterialExpressionMaterialFunctionCall* Call =
				Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
			{
				if (Call->MaterialFunction
					&& Call->MaterialFunction->GetPathName() == CompleteWeatherFoliageFunctionPath)
				{
					CompleteWeatherCall = Call;
				}
			}
			if (UMaterialExpressionStaticSwitchParameter* Switch =
				Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
			{
				if (Switch->ParameterName == TEXT("Uses Simple Wind?"))
				{
					OriginalWindSwitch = Switch;
				}
			}
		}

		if (!CompleteWeatherCall)
		{
			return 0;
		}
		FExpressionInput* Wpo = Material->GetExpressionInputForProperty(MP_WorldPositionOffset);
		if (!Wpo || !OriginalWindSwitch)
		{
			UE_LOG(
				LogWeatherEnvironmentEditor,
				Error,
				TEXT("Could not restore the recorded Uses Simple Wind? WPO source while removing %s from %s."),
				CompleteWeatherFoliageFunctionPath,
				*Material->GetPathName());
			return 0;
		}

		const FString ReplacedWpoSource = Wpo->Expression
			? Wpo->Expression->GetName()
			: TEXT("None");
		Wpo->Connect(0, OriginalWindSwitch);
		UMaterialEditingLibrary::DeleteMaterialExpression(Material, CompleteWeatherCall);
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Display,
			TEXT("Restored %s WPO from %s to Uses Simple Wind? and removed the complete Weather foliage deformation."),
			*Material->GetPathName(),
			*ReplacedWpoSource);
		return 1;
	}

	int32 RetargetExpressions(
		UObject* GraphOwner,
		UMaterialParameterCollection* SourceCollection,
		UMaterialParameterCollection* TargetCollection)
	{
		int32 ChangedCount = 0;
		for (TObjectIterator<UMaterialExpressionCollectionParameter> It; It; ++It)
		{
			UMaterialExpressionCollectionParameter* Parameter = *It;
			if (!Parameter->IsIn(GraphOwner) || !Parameter->Collection)
			{
				continue;
			}

			const bool bUsesSourceCollection =
				Parameter->Collection == SourceCollection ||
				Parameter->Collection->GetPathName() == SourceCollectionPath;
			if (!bUsesSourceCollection)
			{
				continue;
			}

			const FGuid TargetParameterId = TargetCollection->GetParameterId(Parameter->ParameterName);
			if (!TargetParameterId.IsValid())
			{
				UE_LOG(
					LogWeatherEnvironmentEditor,
					Error,
					TEXT("Weather MPC is missing foliage compatibility parameter '%s'."),
					*Parameter->ParameterName.ToString());
				continue;
			}

			Parameter->Modify();
			Parameter->Collection = TargetCollection;
			Parameter->ParameterId = TargetParameterId;
			++ChangedCount;
		}
		return ChangedCount;
	}

	int32 RetargetInstanceOverrides(
		const TArray<UMaterial*>& MasterMaterials,
		UMaterialParameterCollection* SourceCollection,
		UMaterialParameterCollection* TargetCollection)
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
		TSet<FName> ReferencerPackages;
		for (const UMaterial* Material : MasterMaterials)
		{
			TArray<FName> Referencers;
			AssetRegistry.GetReferencers(
				Material->GetOutermost()->GetFName(),
				Referencers,
				UE::AssetRegistry::EDependencyCategory::Package,
				UE::AssetRegistry::EDependencyQuery::Hard);
			ReferencerPackages.Append(Referencers);
		}

		int32 ChangedInstances = 0;
		for (const FName PackageName : ReferencerPackages)
		{
			TArray<FAssetData> Assets;
			AssetRegistry.GetAssetsByPackageName(PackageName, Assets);
			for (const FAssetData& AssetData : Assets)
			{
				UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(AssetData.GetAsset());
				if (!Instance)
				{
					continue;
				}

				bool bChanged = false;
				for (FParameterCollectionParameterValue& Parameter :
					Instance->ParameterCollectionParameterValues)
				{
					if (Parameter.ParameterValue == SourceCollection)
					{
						Instance->Modify();
						Parameter.ParameterValue = TargetCollection;
						bChanged = true;
					}
				}

				if (bChanged)
				{
					UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
					Instance->PostEditChange();
					Instance->MarkPackageDirty();
					if (WeatherWindMaterial::SaveAsset(Instance))
					{
						++ChangedInstances;
					}
				}
			}
		}
		return ChangedInstances;
	}
}

void FWeatherEnvironmentSystemEditorModule::StartupModule()
{
	if (GUnrealEd)
	{
		RegisterComponentVisualizers();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(
			this,
			&FWeatherEnvironmentSystemEditorModule::RegisterComponentVisualizers);
	}

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

	GenerateWindMaterialAssetsCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Weather.GenerateWindMaterialAssets"),
		TEXT("Create or repair the shared Stage 3 wind texture, MPC, and foliage material function."),
		FConsoleCommandDelegate::CreateRaw(this, &FWeatherEnvironmentSystemEditorModule::GenerateWindMaterialAssets),
		ECVF_Default);

	RetargetLegacyFoliageMaterialsCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Weather.RetargetLegacyFoliageMaterials"),
		TEXT("Retarget the established foliage wind functions and their instance overrides to MPC_WeatherEnvironment."),
		FConsoleCommandDelegate::CreateRaw(
			this,
			&FWeatherEnvironmentSystemEditorModule::RetargetLegacyFoliageMaterials),
		ECVF_Default);

	ValidateSpatialFoliageMaterialsCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Weather.ValidateSpatialFoliageMaterials"),
		TEXT("Audit spatial foliage graph wiring and compile all foliage masters and recursive material-instance children."),
		FConsoleCommandDelegate::CreateRaw(
			this,
			&FWeatherEnvironmentSystemEditorModule::ValidateSpatialFoliageMaterials),
		ECVF_Default);
}

void FWeatherEnvironmentSystemEditorModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	if (GUnrealEd && bComponentVisualizersRegistered)
	{
		GUnrealEd->UnregisterComponentVisualizer(
			UWeatherGridDebugComponent::StaticClass()->GetFName());
		bComponentVisualizersRegistered = false;
	}

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

	if (GenerateWindMaterialAssetsCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(GenerateWindMaterialAssetsCommand);
		GenerateWindMaterialAssetsCommand = nullptr;
	}

	if (RetargetLegacyFoliageMaterialsCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(RetargetLegacyFoliageMaterialsCommand);
		RetargetLegacyFoliageMaterialsCommand = nullptr;
	}

	if (ValidateSpatialFoliageMaterialsCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ValidateSpatialFoliageMaterialsCommand);
		ValidateSpatialFoliageMaterialsCommand = nullptr;
	}
}

void FWeatherEnvironmentSystemEditorModule::RegisterComponentVisualizers()
{
	if (!GUnrealEd || bComponentVisualizersRegistered)
	{
		return;
	}

	TSharedPtr<FComponentVisualizer> GridDebugVisualizer =
		MakeShared<FWeatherGridDebugComponentVisualizer>();
	GUnrealEd->RegisterComponentVisualizer(
		UWeatherGridDebugComponent::StaticClass()->GetFName(),
		GridDebugVisualizer);
	GridDebugVisualizer->OnRegister();
	bComponentVisualizersRegistered = true;
}

void FWeatherEnvironmentSystemEditorModule::GenerateWindMaterialAssets()
{
	using namespace WeatherWindMaterial;

	UMaterialParameterCollection* Collection = CreateOrUpdateParameterCollection();
	UTexture2D* FieldTexture = CreateOrLoadFieldTexture();
	if (!Collection || !FieldTexture)
	{
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Error,
			TEXT("Failed to create the Stage 3 parameter collection or field texture."));
		return;
	}

	if (!CreateOrRebuildFoliageFunction(Collection, FieldTexture))
	{
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Error,
			TEXT("Failed to create the Stage 3 foliage material function."));
		return;
	}
	if (!CreateOrRebuildWindSampleFunction(Collection, FieldTexture))
	{
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Error,
			TEXT("Failed to create the Stage 3 sampler-only wind material function."));
		return;
	}

	UE_LOG(
		LogWeatherEnvironmentEditor,
		Display,
		TEXT("Generated Stage 3 assets: %s, %s, %s, and %s."),
		ParameterCollectionPackagePath,
		FieldTexturePackagePath,
		FoliageFunctionPackagePath,
		WindSampleFunctionPackagePath);
}

void FWeatherEnvironmentSystemEditorModule::RetargetLegacyFoliageMaterials()
{
	using namespace WeatherLegacyFoliageIntegration;

	UMaterialParameterCollection* TargetCollection =
		WeatherWindMaterial::CreateOrUpdateParameterCollection();
	UTexture2D* FieldTexture = WeatherWindMaterial::CreateOrLoadFieldTexture();
	UMaterialFunction* WindSampleFunction = TargetCollection && FieldTexture
		? WeatherWindMaterial::CreateOrRebuildWindSampleFunction(TargetCollection, FieldTexture)
		: nullptr;
	UMaterialParameterCollection* SourceCollection =
		LoadObject<UMaterialParameterCollection>(nullptr, SourceCollectionPath);
	if (!TargetCollection || !SourceCollection || !WindSampleFunction)
	{
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Error,
			TEXT("Could not load the source collection, Weather collection, field texture, and sampler function."));
		return;
	}

	int32 ChangedExpressions = 0;
	const int32 UpdatedProfiles = UpgradeMainWorldWindCadence();
	int32 SavedFunctions = 0;
	for (const TCHAR* FunctionPath : FunctionPaths)
	{
		UMaterialFunction* Function = LoadObject<UMaterialFunction>(nullptr, FunctionPath);
		if (!Function)
		{
			UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Missing foliage function %s."), FunctionPath);
			continue;
		}

		Function->Modify();
		const int32 RetargetChanges = RetargetExpressions(Function, SourceCollection, TargetCollection);
		const int32 SpatialChanges = IntegrateSpatialWind(
			Function,
			WindSampleFunction,
			TargetCollection);
		const int32 FunctionChanges = RetargetChanges + SpatialChanges;
		ChangedExpressions += FunctionChanges;
		if (FunctionChanges > 0)
		{
			Function->UpdateFromFunctionResource();
			Function->PostEditChange();
			Function->MarkPackageDirty();
			UMaterialEditingLibrary::UpdateMaterialFunction(Function);
			SavedFunctions += WeatherWindMaterial::SaveAsset(Function) ? 1 : 0;
		}
	}

	const bool bRefreshMastersForFunctionChanges = ChangedExpressions > 0;
	TArray<UMaterial*> MasterMaterials;
	int32 SavedMaterials = 0;
	for (const TCHAR* MaterialPath : MaterialPaths)
	{
		UMaterial* Material = LoadObject<UMaterial>(nullptr, MaterialPath);
		if (!Material)
		{
			UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Missing foliage material %s."), MaterialPath);
			continue;
		}

		MasterMaterials.Add(Material);
		const int32 MaterialChanges = RetargetExpressions(Material, SourceCollection, TargetCollection)
			+ RestorePlantsMasterWpo(Material);
		ChangedExpressions += MaterialChanges;
		if (bRefreshMastersForFunctionChanges || MaterialChanges > 0)
		{
			Material->Modify();
			UMaterialEditingLibrary::RecompileMaterial(Material);
			Material->PostEditChange();
			Material->MarkPackageDirty();
			SavedMaterials += WeatherSkyboxMaterial::SaveMaterial(Material) ? 1 : 0;
		}
	}

	const int32 ChangedInstances = RetargetInstanceOverrides(
		MasterMaterials,
		SourceCollection,
		TargetCollection);
	UE_LOG(
		LogWeatherEnvironmentEditor,
		Display,
		TEXT("Retargeted foliage to %s: %d expression(s), %d function(s), %d refreshed master material(s), %d instance override(s), and %d profile cadence update(s)."),
		WeatherWindMaterial::ParameterCollectionPackagePath,
		ChangedExpressions,
		SavedFunctions,
		SavedMaterials,
		ChangedInstances,
		UpdatedProfiles);
}

void FWeatherEnvironmentSystemEditorModule::ValidateSpatialFoliageMaterials()
{
	using namespace WeatherLegacyFoliageIntegration;

	UMaterialFunction* WindSampleFunction = LoadObject<UMaterialFunction>(
		nullptr,
		TEXT("/WeatherEnvironmentSystem/Materials/MF_WeatherWindSample.MF_WeatherWindSample"));
	UMaterialParameterCollection* OldCollection = LoadObject<UMaterialParameterCollection>(
		nullptr,
		SourceCollectionPath);
	if (!WindSampleFunction || !OldCollection)
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Spatial foliage validation could not load its sampler or legacy MPC."));
		return;
	}

	int32 ErrorCount = 0;
	int32 PhaseStableSamplerNodes = 0;
	for (UMaterialExpression* Expression : WindSampleFunction->GetExpressions())
	{
		const UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression);
		if (Custom && Custom->Code.Contains(TEXT("return float4(Wind.xy, SpeedScale, 1.0)")))
		{
			++PhaseStableSamplerNodes;
		}
	}
	if (PhaseStableSamplerNodes != 1)
	{
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Error,
			TEXT("MF_WeatherWindSample is not using the phase-stable legacy animation rate."));
		++ErrorCount;
	}

	UWeatherEnvironmentProfile* MainProfile = LoadObject<UWeatherEnvironmentProfile>(
		nullptr,
		MainWorldWeatherProfilePath);
	if (!MainProfile)
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Validation could not load %s."), MainWorldWeatherProfilePath);
		++ErrorCount;
	}
	else if (MainProfile->Wind.FixedUpdateIntervalSeconds > SpatialFoliageWindCadence + 0.0001f)
	{
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Error,
			TEXT("Main-world wind cadence is %.3f seconds; spatial foliage requires 30 Hz (0.033 seconds) or faster."),
			MainProfile->Wind.FixedUpdateIntervalSeconds);
		++ErrorCount;
	}
	else
	{
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Display,
			TEXT("Main-world wind cadence validated at %.3f seconds."),
			MainProfile->Wind.FixedUpdateIntervalSeconds);
	}

	for (const TCHAR* FunctionPath : FunctionPaths)
	{
		UMaterialFunction* Function = LoadObject<UMaterialFunction>(nullptr, FunctionPath);
		if (!Function)
		{
			UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Validation could not load %s."), FunctionPath);
			++ErrorCount;
			continue;
		}

		int32 SampleCalls = 0;
		int32 LegacyCollectionNodes = 0;
		int32 RotateAboutAxisNodes = 0;
		int32 PhaseStableGustOrientations = 0;
		for (UMaterialExpression* Expression : Function->GetExpressions())
		{
			if (UMaterialExpressionMaterialFunctionCall* Call =
				Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
			{
				SampleCalls += Call->MaterialFunction == WindSampleFunction ? 1 : 0;
			}
			if (UMaterialExpressionCollectionParameter* Parameter =
				Cast<UMaterialExpressionCollectionParameter>(Expression))
			{
				LegacyCollectionNodes += Parameter->Collection == OldCollection ? 1 : 0;
			}
			RotateAboutAxisNodes += Cast<UMaterialExpressionRotateAboutAxis>(Expression) ? 1 : 0;
			if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression))
			{
				PhaseStableGustOrientations +=
					Custom->Desc == TEXT("Weather spatial gust-wind orientation (generated)")
						&& Custom->Code == PhaseStableGustOrientationCode
					? 1 : 0;
			}
		}

		if (SampleCalls != 1)
		{
			UE_LOG(
				LogWeatherEnvironmentEditor,
				Error,
				TEXT("%s has %d spatial sampler calls; expected exactly one."),
				FunctionPath,
				SampleCalls);
			++ErrorCount;
		}
		if (LegacyCollectionNodes != 0)
		{
			UE_LOG(
				LogWeatherEnvironmentEditor,
				Error,
				TEXT("%s still contains %d reference(s) to the old foliage MPC."),
				FunctionPath,
				LegacyCollectionNodes);
			++ErrorCount;
		}
		if (Function->GetPathName().Contains(TEXT("MF_FoliageWind_Sway"))
			&& RotateAboutAxisNodes == 0)
		{
			UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("The authored sway RotateAboutAxis node is missing."));
			++ErrorCount;
		}
		if (Function->GetPathName().Contains(TEXT("MF_GustingWind"))
			&& PhaseStableGustOrientations != 1)
		{
			UE_LOG(
				LogWeatherEnvironmentEditor,
				Error,
				TEXT("The gusting function has %d phase-stable spatial orientation nodes; expected one."),
				PhaseStableGustOrientations);
			++ErrorCount;
		}
	}

	UMaterialFunction* CompleteWindFunction = LoadObject<UMaterialFunction>(
		nullptr,
		CompleteWeatherFoliageFunctionPath);
	TArray<UMaterialInterface*> InterfacesToCompile;
	TArray<UMaterialInterface*> ParentQueue;
	for (const TCHAR* MaterialPath : MaterialPaths)
	{
		UMaterial* Material = LoadObject<UMaterial>(nullptr, MaterialPath);
		if (!Material)
		{
			UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Validation could not load %s."), MaterialPath);
			++ErrorCount;
			continue;
		}

		int32 CompleteWindCalls = 0;
		int32 TreeSwayCalls = 0;
		int32 TreeRustleCalls = 0;
		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			UMaterialExpressionMaterialFunctionCall* Call =
				Cast<UMaterialExpressionMaterialFunctionCall>(Expression);
			if (!Call || !Call->MaterialFunction)
			{
				continue;
			}
			CompleteWindCalls += Call->MaterialFunction == CompleteWindFunction ? 1 : 0;
			TreeSwayCalls += Call->MaterialFunction->GetPathName().Contains(TEXT("MF_FoliageWind_Sway")) ? 1 : 0;
			TreeRustleCalls += Call->MaterialFunction->GetPathName().Contains(TEXT("MF_FoliageWind_Rustle")) ? 1 : 0;
		}
		if (CompleteWindCalls != 0)
		{
			UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("%s still calls the complete Weather foliage WPO."), MaterialPath);
			++ErrorCount;
		}
		if (Material->GetPathName().Contains(TEXT("M_Tree_GlobalWind_Master"))
			&& (TreeSwayCalls != 2 || TreeRustleCalls != 1))
		{
			UE_LOG(
				LogWeatherEnvironmentEditor,
				Error,
				TEXT("Tree master authored wind chain changed unexpectedly: %d sway and %d rustle calls."),
				TreeSwayCalls,
				TreeRustleCalls);
			++ErrorCount;
		}
		if (Material->GetPathName().Contains(TEXT("M_Plants_Master")))
		{
			FExpressionInput* Wpo = Material->GetExpressionInputForProperty(MP_WorldPositionOffset);
			UMaterialExpressionStaticSwitchParameter* Switch = Wpo
				? Cast<UMaterialExpressionStaticSwitchParameter>(Wpo->Expression)
				: nullptr;
			if (!Switch || Switch->ParameterName != TEXT("Uses Simple Wind?"))
			{
				UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Plant master WPO is not restored to Uses Simple Wind?."));
				++ErrorCount;
			}
		}

		UMaterialEditingLibrary::RecompileMaterial(Material);
		InterfacesToCompile.Add(Material);
		ParentQueue.Add(Material);
	}

	TSet<FSoftObjectPath> VisitedInstances;
	for (int32 QueueIndex = 0; QueueIndex < ParentQueue.Num(); ++QueueIndex)
	{
		TArray<FAssetData> Children;
		UMaterialEditingLibrary::GetChildInstances(ParentQueue[QueueIndex], Children);
		for (const FAssetData& ChildData : Children)
		{
			const FSoftObjectPath ChildPath = ChildData.GetSoftObjectPath();
			if (VisitedInstances.Contains(ChildPath))
			{
				continue;
			}
			VisitedInstances.Add(ChildPath);
			UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(ChildData.GetAsset());
			if (!Instance)
			{
				continue;
			}
			UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
			InterfacesToCompile.Add(Instance);
			ParentQueue.Add(Instance);
		}
	}

	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}

	for (UMaterialInterface* Interface : InterfacesToCompile)
	{
		const FMaterialResource* Resource = Interface->GetMaterialResource(GMaxRHIShaderPlatform);
		if (!Resource)
		{
			continue;
		}
		for (const FString& CompileError : Resource->GetCompileErrors())
		{
			UE_LOG(
				LogWeatherEnvironmentEditor,
				Error,
				TEXT("Material compile error in %s: %s"),
				*Interface->GetPathName(),
				*CompileError);
			++ErrorCount;
		}
	}

	if (ErrorCount == 0)
	{
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Display,
			TEXT("Spatial foliage validation passed: 4 established functions, 3 masters, and %d recursive material-instance children compiled without errors."),
			VisitedInstances.Num());
	}
	else
	{
		UE_LOG(LogWeatherEnvironmentEditor, Error, TEXT("Spatial foliage validation failed with %d error(s)."), ErrorCount);
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
