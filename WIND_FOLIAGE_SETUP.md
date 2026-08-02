# Weather wind and foliage setup

Stage 3 supplies one movable wind director, fixed-step cell wind simulation, a shared world-space field texture, player-local fallback values, and a reusable foliage material function. It does not create per-cell actors or update per-foliage material instances.

## 1. Place the wind director

1. Place one **Weather Wind Director** in the level covered by the Weather Environment Controller.
2. Assign it to the controller's **Wind Director** reference. When **Auto Discover Wind Director** is enabled, the controller first prefers an actor tagged `WeatherWindDirector`, then uses the first director it finds.
3. Choose **Manual Transform** to position the actor directly, or **Spline Route** to use its route.

Each cell's base direction points from the cell centre toward the director. Moving the director therefore rotates the complete field coherently. Inside **Director Dead Zone Radius**, a cell retains its previous valid direction; if none exists, it uses the director's forward vector.

## 2. Configure a route

For **Spline Route** mode, add route points to the director. Locations are world-space destinations and the component spline visualizes the route.

Each point provides:

- **Travel Speed** in cm/s;
- **Arrival Radius**;
- **Pause Duration**;
- **Easing**: Linear, Ease In, Ease Out, or Ease In Out.

Set **Route Behavior** to Once, Loop, or Ping Pong. **Start Route On Begin Play** starts it automatically; Blueprints can also call `Set Director Route`, `Start Route`, and `Stop Route`, then query running, paused, target-point, and complete route state.

## 3. Tune the profile

Open the assigned Weather Environment Profile and expand **Wind**.

- **Fixed Update Interval Seconds** controls authoritative updates; the foliage-ready default is `0.0333` seconds (30 Hz).
- **Base Wind Speed** is stored in cell vectors as cm/s.
- **Maximum Wind Speed** clamps simulation speed and normalizes the texture B channel.
- **Direction Smoothing Rate** controls exponential interpolation. Zero applies changes immediately.
- **Curl Noise Strength** adds optional large-scale direction breakup. Zero preserves the exact director direction.
- **Gust Strength**, **Gust Speed Multiplier**, frequency, and scale control the normalized gust signal and its speed contribution.
- **Field Texture** and **Material Parameter Collection** default to the supplied plugin assets. Keep those defaults unless a replacement material contract is intentional.

The field texture uploads only after fixed simulation steps and only when encoded pixels changed. Keep the interval at `0.0333` seconds or faster when the director moves continuously; slower updates are appropriate for gameplay simulation but visibly step a vector used directly by tree WPO. Large direction changes are smoothed as an angle and speed rather than interpolating through a zero-length vector.

The Weather Environment Profile is the authoring source. The subsystem writes its evaluated values into `MPC_WeatherEnvironment` while the game runs, so editing MPC defaults during PIE will be overwritten and is not a tuning workflow. Change **Wind** or **Wind > Foliage Materials** on the profile assigned to the controller. Restart PIE, or call **Refresh Environment** on the controller when applying a profile change at runtime.

## 4. Existing project foliage integration

The established project foliage graphs keep their current World Position Offset logic while sampling the Weather spatial wind field at each foliage object's location. Run the editor migration command once:

```text
Weather.RetargetLegacyFoliageMaterials
```

It retargets collection-parameter nodes from `/Game/Level_Building/Foliage_EnvironmentSettings` to `/WeatherEnvironmentSystem/Materials/MPC_WeatherEnvironment` and integrates the sampler-only `/WeatherEnvironmentSystem/Materials/MF_WeatherWindSample` into:

- `MF_GustingWind`;
- `MF_SimpleWInd`;
- `MF_FoliageWind_Rustle`;
- `MF_FoliageWind_Sway`.

It then refreshes `M_Grass_Master`, `M_Plants_Master`, and `M_Tree_GlobalWind_Master`, and replaces any matching collection overrides on their direct material-instance children. The old project MPC is no longer the source of foliage WPO after migration.

`MF_WeatherWindSample` performs no deformation. It samples the field at `ObjectPositionWS`, so one tree receives one coherent direction across all of its vertices, then returns direction, a legacy-compatible displacement scale, and a phase-stable rate scale.

The authored graph behavior is preserved for visual continuity:

- tree branches and trunks retain the same pivot, rotate-about-axis sway, squared pre-skinned-height fade, and authored animation rate while direction and displacement strength come from their location;
- leaves use the same location-based sway as their branches, with their existing vertex-colour rustle mask, authored rustle rate, and spatially scaled rustle displacement added;
- plants retain their existing choice between simple wind and two-scale gusting wind; both are oriented into local wind and scale displacement spatially, while their authored time rates stay continuous;
- all existing per-instance influence, speed, falloff, and static-switch controls remain active.

The controller still publishes player-local compatibility values for materials that cannot spatially sample. The migrated functions instead combine unscaled authored values with their own field sample, preventing double scaling. At the default **Reference Wind Speed** of 500 cm/s, the spatial displacement matches the old MPC defaults. Wind speed—including the gust contribution to simulated speed—scales displacement. Authored sway, rustle, simple-wind, and noise-pan rates stay constant because those legacy functions multiply the rate by absolute material time; changing the rate at runtime would rewrite their historical phase and cause WPO jumps. Expand **Wind > Foliage Materials** on the Weather Environment Profile to tune the base values without editing the MPC.

## 5. Opt new or otherwise unconfigured foliage materials in

The plugin asset `/WeatherEnvironmentSystem/Materials/MF_WeatherFoliageWind` contains the complete sampling and motion contract.

Do not add this complete WPO function to `M_Tree_GlobalWind_Master`, `M_Plants_Master`, or `M_Grass_Master`. Those materials already contain tuned wind deformation, and adding this function would apply a second, incompatible deformation while bypassing their established height fades, masks, switches, and instance controls. Their existing functions already contain `MF_WeatherWindSample` after the migration in section 4.

Use `MF_WeatherFoliageWind` for a material that does not already have wind WPO:

1. Open the foliage parent material.
2. Add a Material Function Call using `MF_WeatherFoliageWind`.
3. Connect its **World Position Offset** output directly to WPO. If the material has unrelated non-wind deformation, add only that unrelated deformation to the function output.
4. Leave the function's `WeatherFieldTexture` parameter on `/WeatherEnvironmentSystem/Materials/T_WeatherWindField`. The subsystem updates this one shared texture in place, so foliage MIDs do not need updates.
5. Paint or supply vertex colour masks when art direction requires them: vertex **R** controls branch flexibility and vertex **A** controls leaf rustle. Unpainted white vertex colour enables both.

Material-instance parameters exposed by the function are packed as follows:

- `WeatherFoliageSway`: X trunk amplitude in cm, Y branch amplitude in cm, Z slow sway frequency in Hz, W height in cm at which bend reaches full strength.
- `WeatherFoliageRustle`: X rustle amplitude in cm, Y rustle frequency in Hz, Z spatial wavelength in cm, W vertical-displacement fraction.

The defaults give slow trunk motion and a separate higher-frequency leaf signal.

## Material contract

The shared 64×64 `WeatherFieldTexture` encodes:

- R/G: normalized XY direction mapped from `-1..1` to `0..1`;
- B: speed divided by the profile's maximum wind speed;
- A: normalized gust.

`MPC_WeatherEnvironment` publishes:

- `WeatherFieldOriginSize`: XY world origin and ZW world size;
- `WeatherLocalWind`: XY direction, Z speed in cm/s, W normalized speed;
- `WeatherLocalGust`;
- `WeatherLocalRain`;
- `WeatherLocalStorminess`.

For spatial evaluation of the established foliage curves it also publishes:

- `WeatherFoliageMapping`: maximum/reference speed conversion, a reserved gust-response component, and the sway-axis convention;
- `WeatherFoliageBase`: authored simple-wind and tree-sway intensity/rate values;
- `WeatherFoliageGustBase`: authored two-scale gust amplitudes and wavelengths.

For migrated project foliage it also publishes the exact legacy parameter names used by the established functions:

- `Grass Wind Small Size` and `Grass Wind Large Size`;
- `Grass Wind Small Amplification` and `Grass Wind Large Amplification`;
- `Simple Wind Intensity` and `Simple Wind Speed`;
- `Wind Sway Direction`, `Wind Sway Intensity`, `Wind Sway Gust Frequency`, `Wind Sway Gradient`, and `Wind Sway Offset`.

Unreal Material Parameter Collections support scalar and vector parameters only. The texture is therefore the shared `WeatherFieldTexture` texture parameter embedded in the material function; all mapping and fallback values come from the one MPC. Materials that cannot adopt the function can read the `WeatherLocal*` MPC values directly.

## Blueprint and C++ queries

`WeatherStateSubsystem` and the controller expose `Get Wind At Location`. The subsystem also exposes `Set Wind Director`, `Force Wind Update`, `Get Wind Field Texture`, and `Get Wind Field Origin Size`.

The complete cell vector used by queries is also the exact vector drawn by Stage 2 debug wind arrows.

## Rebuild the supplied assets

The Stage 3 content assets are reproducible. In the editor console, run:

```text
Weather.GenerateWindMaterialAssets
```

This creates or repairs `MPC_WeatherEnvironment`, `T_WeatherWindField`, and `MF_WeatherFoliageWind` under the plugin's Materials folder.

After generating the plugin assets, run the project-foliage migration once:

```text
Weather.RetargetLegacyFoliageMaterials
```

To audit graph wiring and compile the masters plus all recursive material-instance children, run:

```text
Weather.ValidateSpatialFoliageMaterials
```

## Validation

Enable grid debug wind arrows, then move the director through the grid in PIE. Arrows should turn smoothly, with cells inside the dead zone retaining their last direction. A foliage material using the function should show low-frequency trunk/branch sway and independently adjustable leaf rustle.

Automation tests are registered under:

```text
WeatherEnvironment.Stage3
```
