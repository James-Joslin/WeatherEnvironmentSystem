// Copyright James Joslin. All Rights Reserved.

#include "WeatherEnvironmentSystemEditorModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "HAL/IConsoleManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionSkyAtmosphereViewLuminance.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"

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

	UE_LOG(
		LogWeatherEnvironmentEditor,
		Display,
		TEXT("Generated Stage 3 assets: %s, %s, and %s."),
		ParameterCollectionPackagePath,
		FieldTexturePackagePath,
		FoliageFunctionPackagePath);
}

void FWeatherEnvironmentSystemEditorModule::RetargetLegacyFoliageMaterials()
{
	using namespace WeatherLegacyFoliageIntegration;

	UMaterialParameterCollection* TargetCollection =
		WeatherWindMaterial::CreateOrUpdateParameterCollection();
	UMaterialParameterCollection* SourceCollection =
		LoadObject<UMaterialParameterCollection>(nullptr, SourceCollectionPath);
	if (!TargetCollection || !SourceCollection)
	{
		UE_LOG(
			LogWeatherEnvironmentEditor,
			Error,
			TEXT("Could not load both source and Weather foliage parameter collections."));
		return;
	}

	int32 ChangedExpressions = 0;
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
		const int32 FunctionChanges = RetargetExpressions(Function, SourceCollection, TargetCollection);
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
		const int32 MaterialChanges = RetargetExpressions(Material, SourceCollection, TargetCollection);
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
		TEXT("Retargeted foliage to %s: %d expression(s), %d function(s), %d refreshed master material(s), and %d instance override(s)."),
		WeatherWindMaterial::ParameterCollectionPackagePath,
		ChangedExpressions,
		SavedFunctions,
		SavedMaterials,
		ChangedInstances);
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
