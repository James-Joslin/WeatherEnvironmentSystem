# Custom skybox post-process setup

> **Legacy fallback:** the recommended cloud-compatible implementation is now the opaque `M_WeatherSkyDome` material documented in `SKY_DOME_SETUP.md`. Do not install this post-process material while the sky dome is active; it will composite over volumetric clouds.

The supplied shader replaces the large node chain in PostProcess_Stars_Mat2 while retaining four independently graded cubemaps, a shaped world-space vertical gradient, optional tinting, atmospheric fade, and scene-color preservation.

The important correction is that it does not use Reflection Vector or divide/floor a very large linear depth. It reconstructs a projection-correct world ray from the current view and identifies empty sky using Unreal's raw reversed-Z far plane. Both calculations remain valid in viewport corners above 90 degrees FOV.

## Master material

Create or duplicate a material inside the WeatherEnvironmentSystem plugin and configure:

- Material Domain: Post Process
- Blendable Location: Scene Color After DOF (the UE 5.7 name for the former Before Tonemapping location)
- Output Alpha: disabled
- Connect the Custom node output to Emissive Color
- Custom node Output Type: CMOT Float 4
- Custom node Include File Paths: /WeatherEnvironmentSystem/Private/WeatherSkyboxCommon.ush

Create these Custom node inputs. Texture inputs must be Texture Object Parameter nodes whose parameter names match the list.

| Custom input | Material expression and parameter |
|---|---|
| SceneColor | SceneTexture: PostProcessInput0, Color |
| LinearSceneDepth | SceneTexture: SceneDepth, Color |
| AtmosphereLuminance | SkyAtmosphereViewLuminance, RGB |
| WeatherSkybox1..4 | Four Texture Object Parameter Cube nodes |
| HueShifts | Vector Parameter WeatherHueShifts, RGBA output |
| Saturations | Vector Parameter WeatherSaturations, RGBA output |
| Luminosities | Vector Parameter WeatherLuminosities, RGBA output |
| MipLevels | Vector Parameter WeatherMipLevels, RGBA output |
| GradientParameters | Vector Parameter WeatherGradientParams, RGBA output |
| GradientColor | Vector Parameter WeatherGradientColor, RGBA output |
| Tint | Vector Parameter WeatherTint, RGBA output |
| TintBlend | Scalar Parameter WeatherTintBlend |
| MaximumBrightness | Scalar Parameter WeatherMaxBrightness |
| SkyRotationDegrees | Scalar Parameter WeatherSkyRotationDegrees |
| DepthParameters | Vector Parameter WeatherDepthParams, RGBA output |
| AtmosphereParameters | Vector Parameter WeatherAtmosphereParams, RGBA output |
| DayNightParameters | Vector Parameter WeatherDayNightParams, RGBA output |

Paste this as the complete Custom node code:

    return WeatherEvaluateSkybox(
        Parameters,
        SceneColor,
        LinearSceneDepth,
        AtmosphereLuminance,
        WeatherSkybox1, WeatherSkybox1Sampler,
        WeatherSkybox2, WeatherSkybox2Sampler,
        WeatherSkybox3, WeatherSkybox3Sampler,
        WeatherSkybox4, WeatherSkybox4Sampler,
        HueShifts,
        Saturations,
        Luminosities,
        MipLevels,
        GradientParameters,
        GradientColor,
        Tint,
        TintBlend,
        MaximumBrightness,
        SkyRotationDegrees,
        DepthParameters,
        AtmosphereParameters,
        DayNightParameters);

The three Scene/SkyAtmosphere expressions must remain connected. Besides supplying their values, they tell the material compiler which renderer resources this custom code needs.

The editor module also registers Weather.GenerateSkyboxMaterial. Running that console command creates /WeatherEnvironmentSystem/Materials/M_WeatherSkyboxPostProcess when it is missing, copies the four default cubemaps it can find in the legacy PostProcess_Stars_Mat2 asset, builds this compact graph, compiles it, and saves it inside the new plugin. If the asset already exists, the command repairs the packed vector connections, recompiles, validates, and saves it. Delete the generated asset first only when you intentionally want to rebuild its complete graph from the legacy source.

## Runtime use

The recommended runtime path is controller-managed: the controller creates and updates one MID inside its own unbound Post Process Component. A manually created Material Instance can instead be added to an existing unbound Post Process Volume, but it remains static and must not be active at the same time as the controller-managed sky. See `IN_ENGINE_SETUP_GUIDE.md` for both workflows and the complete Stage 1 level setup.

1. Create a WeatherEnvironmentProfile data asset.
2. Assign the new post-process master material to Skybox > Post Process Material.
3. Assign up to four cubemaps and tune each hue, saturation, luminosity, mip, and optional day-fraction luminosity curve.
4. Assign the profile to one WeatherEnvironmentController in the persistent/open-world map.
5. Disable the old PostProcess_Stars_Mat2 blendable to avoid evaluating both sky replacements.

The controller creates the MID and writes all Weather-prefixed parameters. WeatherDepthParams contains raw device-Z epsilon, feather width, and the optional finite sky-dome linear-depth fallback. Start with the profile defaults. If very distant geometry is incorrectly classified as sky, reduce the epsilon or set Linear Depth Fallback Distance to zero.

WeatherDayNightParams contains the full-night solar elevation, full-day solar elevation, and direction-gate strength. The shader reads Sky Atmosphere light 0's world direction, so even a manually installed Material Instance hides the night sky during daytime. View luminance remains a secondary atmospheric blend rather than the source of day/night state. The controller can additionally gate Atmosphere Fade Strength by its astronomical sun elevation or an Atmosphere Fade By Sun Elevation curve.

## FOV validation

Test horizontal FOV values 60, 90, 120, and 150 while looking at all four viewport corners. The custom sky should cover only far-plane pixels, and nearby geometry must retain PostProcessInput0. Test with screen percentage/TAA or TSR settings used by the game because GetDefaultSceneTextureUV handles their buffer-to-viewport mapping.
