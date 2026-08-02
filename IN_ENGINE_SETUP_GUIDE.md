# Weather Environment System — in-engine setup guide

This guide covers the clock and sky setup introduced in Stage 1 of the Weather Environment System plugin. Stages 2 and 3 additionally provide:

- persistent session clock with pause and time scaling;
- astronomical sun and moon directions;
- runtime control of sun and moon Directional Lights;
- an orbiting moon static mesh;
- a controller-managed, four-cubemap opaque sky dome with per-layer grading;
- Blueprint-readable clock, astronomy values, and transition events.
- a landscape/manual-bounds weather grid and debug view;
- a movable or routed wind director;
- a shared wind field and foliage material function.

For grid, wind director, and foliage setup, continue with [WIND_FOLIAGE_SETUP.md](WIND_FOLIAGE_SETUP.md). Precipitation, weather fronts, local volumetric clouds, and the Ocean System bridge belong to later stages.

## Sky material: short answer

Do not put the new sky-dome material in a Post Process Volume. `M_WeatherSkyDome` is a Surface-domain, Opaque, Unlit, Two Sided material with **Is Sky** enabled. The Weather Environment Controller assigns it to its own large sphere and keeps that sphere centred on the player camera. The opaque sky is composed before volumetric clouds, so clouds can appear naturally in front of stars.

Remove `M_WeatherSkyboxPostProcess` or any instance derived from it from your unbound Post Process Volume. That material is now a legacy fallback only. If both paths are active, the post-process sky will still paint over the clouds.

## 1. Enable and locate the plugin

1. Open **Edit > Plugins**.
2. Search for **Weather Environment System** and enable it.
3. Restart the editor if prompted.
4. In the Content Browser settings, enable **Show Plugin Content**.
5. Confirm that this asset is visible:

   `/WeatherEnvironmentSystem/Materials/M_WeatherSkyDome`

If the material is missing, open the editor console and run:

```text
Weather.GenerateSkyDomeMaterial
```

The command creates or repairs the compact sky-dome master material and copies the four default cubemaps from the legacy `PostProcess_Stars_Mat2` asset when that asset is available.

## 2. Create an environment profile

Store the game-specific profile in the project's normal Content folder so plugin upgrades do not replace it.

1. In the Content Browser, choose **Add > Miscellaneous > Data Asset**.
2. Select **WeatherEnvironmentProfile**.
3. Name it something such as `DA_MainWorld_WeatherEnvironment`.
4. Open the asset and configure the following sections.

### Clock

- **Start Date Time:** the initial local calendar date and clock time for a new GameInstance.
- **Time Scale:** in-world seconds advanced per real second.
- **Start Paused:** prevents automatic clock advancement until resumed.

Useful Time Scale examples:

| Time Scale | Result |
|---:|---|
| `1` | real-time clock |
| `60` | one game minute per real second; a game day lasts 24 real minutes |
| `600` | ten game minutes per real second; a game day lasts 2.4 real minutes |
| `3600` | one game hour per real second; a game day lasts 24 real seconds |

### Astronomy

- **Latitude Degrees, Longitude Degrees, UTC Offset Hours:** control the calculated solar path for the chosen date.
- **North Yaw Degrees:** rotates astronomical north around Unreal world Z without changing sun elevation.
- **Drive Sun Light / Drive Moon Light:** allow the controller to rotate and grade the corresponding lights.
- **Configure Atmosphere Light Indices:** assigns index 0 to the sun and index 1 to the moon.
- **Sun/Moon Maximum Intensity:** maximum Directional Light intensity before the elevation curve multiplier.
- **Sun/Moon Intensity By Elevation:** optional Curve Float assets evaluated in degrees of elevation.
- **Sun/Moon Color By Day Fraction:** optional Linear Color curves evaluated from `0` to `1` over a day.
- **Sunrise Elevation Degrees:** threshold used for the controller's sunrise and sunset events.
- **Recapture Sky Light:** periodically calls a Skylight recapture. Keep this off when using Skylight Real Time Capture.

Normalized day-fraction keys are `0.0` at midnight, `0.25` at 06:00, `0.5` at noon, `0.75` at 18:00, and `1.0` at the next midnight.

### Moon visual

- **Enabled:** displays the controller's Moon Mesh component.
- **Moon Mesh:** assign a stylised disc, sphere, or custom moon mesh. The Engine basic sphere is the default.
- **Orbit Radius:** distance from the selected orbit centre.
- **Uniform Scale:** world scale applied to the mesh component.
- **Center Orbit On Player Camera:** recommended for an open world because it removes moon parallax as the player travels.
- **Face Orbit Center:** makes the mesh face the camera/orbit centre.
- **Facing Rotation Offset:** corrects the local forward axis of custom moon meshes.

The version-one moon direction is exactly opposite the sun. `Moon Phase` is calculated and exposed to Blueprint and the sky MID, but the plugin does not yet create a phase-aware MID for the Moon Mesh. A custom moon material must therefore implement its own phase appearance if required.

### Skybox

- **Enabled:** enables the controller-owned sky presentation.
- **Use Legacy Post Process Sky:** leave this disabled. Enable it only as a temporary compatibility fallback when the opaque dome cannot be used.
- **Sky Dome Material:** may be left empty. The controller then loads `M_WeatherSkyDome` automatically. It may instead reference a Material Instance derived from that master.
- **Sky Dome Radius:** radius in centimetres of the controller-owned sphere. The default is `10,000,000` cm (100 km).
- **Center Sky Dome On Player Camera:** recommended for the open world so the sphere cannot be approached and sky sampling does not develop travel parallax.
- **Sky Atmosphere Luminance Multiplier:** controls how much Sky Atmosphere view luminance is added behind the volumetric clouds.
- **Atmosphere Fade Strength / Atmosphere Fade By Sun Elevation:** globally scale that atmosphere contribution and optionally art-direct it across solar elevation.
- **Layers 0–3:** the four cubemaps and their independent hue, saturation, luminosity, mip level, and optional luminosity-by-day-fraction curve.
- **Gradient:** applies a projection-independent world-Z colour gradient.
- **Tint:** applies the selected tint according to Tint alpha and Tint Blend.
- **Maximum Brightness:** clamps the custom sky before compositing.
- **Rotation Degrees:** rotates all cubemap sampling around world Z.
- **Day Night:** defines the solar-elevation interval over which the custom night sky fades out. Defaults are fully visible at or below `-6` degrees and fully hidden at or above `0` degrees.
- **Post Process Material, Depth, Blend Weight, and Post Process Priority:** affect only the legacy post-process path.

The generated master contains the four cubemap defaults copied from the legacy material. Leaving a layer's Cubemap empty retains the corresponding default in the master or parent Material Instance.

## 3. Prepare the level lighting

The recommended level contains:

- one **Directional Light** for the sun;
- one **Directional Light** for the moon;
- one **Sky Atmosphere**;
- one **Sky Light**;
- one **Volumetric Cloud** when clouds are required;
- your existing unbound **Post Process Volume** for exposure, colour grading, bloom, depth of field, and similar effects;
- one **Weather Environment Controller**.

Set both Directional Lights to **Movable** so their rotation, intensity, and colour can change during play.

For dependable binding, assign the light references directly on the Weather Environment Controller. Auto-discovery is also available and checks, in order:

1. actor tag `WeatherSun` or `WeatherMoon`;
2. Atmosphere Sun Light index 0 or 1;
3. remaining Directional Lights as a fallback.

When using tags, add the tag in the Directional Light's **Actor > Tags** array. The sun should be Atmosphere Sun Light index 0 and the moon index 1. The controller can configure those indices at runtime when **Configure Atmosphere Light Indices** is enabled.

Use either Skylight **Real Time Capture** or the profile's periodic **Recapture Sky Light** option. Do not enable both unless there is a specific visual reason to do so.

## 4. Place the controller

1. Open the Place Actors panel and search for **Weather Environment Controller**.
2. Place exactly one controller in the open-world level.
3. Assign `DA_MainWorld_WeatherEnvironment` to **Environment Profile**.
4. Assign **Sun Light**, **Moon Light**, and **Sky Light** explicitly, or retain **Auto Discover Lights**.
5. Leave **Moon Orbit Center** empty for camera-centred orbiting. Assign an actor only if a fixed world orbit centre is desired.
6. Select the controller's **Moon Mesh** component if you need to assign moon materials directly to the component.
7. The **Sky Dome Mesh** component is configured automatically at runtime; do not assign collision or a lit material to it.
8. Press Play and use an accelerated Time Scale while tuning.

The fallback Clock, Astronomy, Moon Visual, and Skybox fields on the actor are used only when no Environment Profile is assigned.

Only one controller can be active in a GameInstance at a time. A duplicate logs an error and disables its tick. When using separate interior levels, allow the old level controller to unregister before the new level controller registers.

## 5. Remove conflicts with the old system

Before testing:

1. Disable the old Blueprint Timeline that rotates the sun and moon.
2. Remove the old `PostProcess_Stars_Mat2` blendable from the level Post Process Volume.
3. Check that no Level Blueprint or other day/night actor writes rotation or intensity to the same lights.
4. Remove every Material Instance derived from `M_WeatherSkyboxPostProcess` from the unbound Post Process Volume.
5. Retain the Post Process Volume itself for exposure, colour grading, bloom, depth of field, and all other non-sky settings.

## 6. Optional customised sky-dome instance

Use this when you want asset-level defaults in addition to the values stored in the Weather Environment Profile.

1. Right-click `M_WeatherSkyDome` and select **Create Material Instance**.
2. Save the instance in the project's Content folder, for example `MI_MainWorld_WeatherSky`.
3. Assign the instance to **Skybox > Sky Dome Material** in the profile.
4. Keep **Use Legacy Post Process Sky** disabled.
5. Do not add this instance to a Post Process Volume; it is a Surface material.

The controller creates a runtime MID from the assigned material or instance and continues to apply the profile's `Weather...` values and day-fraction curves. Profile values therefore take precedence for parameters the controller drives.

## 7. Blueprint clock and UI setup

The clock lives in **Weather State Subsystem**, a Game Instance Subsystem. In Blueprint:

1. Use **Get Game Instance Subsystem** and select `WeatherStateSubsystem`.
2. Call **Get Clock Display String** for an `HH:MM` or `HH:MM:SS` UI string.
3. Bind to **On Minute Changed** instead of updating the text every frame.

Available clock calls include:

- **Get Current Date Time**;
- **Get Normalized Day Fraction**;
- **Set Date Time**;
- **Advance World Seconds**;
- **Set Time Scale / Get Time Scale**;
- **Set Clock Paused / Is Clock Paused**.

The controller additionally exposes:

- **Get Sun Direction** and **Get Sun Elevation Degrees**;
- **Get Moon Direction** and **Get Moon Phase**;
- **Get Skybox Material Instance**;
- **On Sunrise** and **On Sunset** events;
- **Refresh Environment**, after references or the profile are changed at runtime.

The subsystem persists through normal level travel while the same GameInstance remains alive. It intentionally resets when Play is stopped or the application restarts; SaveGame persistence is not part of Stage 1.

## 8. Skybox tuning order

For predictable tuning:

1. Set Sky Atmosphere Luminance Multiplier and Directional Daylight Fade Strength to `0` temporarily so only the custom cubemap sky is visible.
2. Tune the four cubemap luminosities, followed by saturation and hue.
3. Tune sky rotation.
4. Add the world-Z gradient and optional tint.
5. Set Maximum Brightness.
6. Re-enable Directional Daylight Fade Strength, then tune Sky Atmosphere Luminance Multiplier, Atmosphere Fade Strength, and the optional sun-elevation curve.
7. Add layer luminosity-by-day-fraction curves last.

The sky material samples with the normalized camera vector on a camera-centred sphere. It has no scene-depth mask and therefore has no wide-FOV corner threshold to fail.

## 9. Validation checklist

Run these checks in PIE or Standalone:

- At Time Scale `3600`, the sun and moon complete a smooth day in about 24 real seconds.
- The moon mesh stays aligned with the moon direction and does not develop parallax as the player moves.
- Sun and moon intensity become appropriate below their horizons; add elevation curves for art-directed transitions.
- The UI clock changes and receives minute-boundary events.
- The custom sky covers the centre and all four corners at horizontal FOV `60`, `90`, `120`, and `150`.
- Nearby geometry remains in front of the opaque sky sphere at every tested FOV.
- Volumetric clouds remain in front of the stars at night and stay visible with art-directed moon intensity.
- Opening an interior level and returning with the same GameInstance does not reset the clock.
- No legacy custom-sky blendable is active.

Automation tests for the implemented stages appear under:

```text
WeatherEnvironment.Stage1
WeatherEnvironment.Stage2
WeatherEnvironment.Stage3
```

Open **Tools > Test Automation**, filter for `WeatherEnvironment`, and run the complete suite.

## 10. Troubleshooting

### The sky renders twice or looks over-bright

Remove every instance of `M_WeatherSkyboxPostProcess` and the old stars material from Post Process Volumes. Keep **Use Legacy Post Process Sky** disabled.

### The sky is black

Check that at least one cubemap layer has non-zero Luminosity, Maximum Brightness is above zero, Skybox is enabled, and the supplied sky-dome material compiles. The dome is intentionally hidden outside play until its runtime MID is created.

### The night sky remains visible during daytime

Use full-night elevation `-6`, fully-hidden elevation `0`, and direction-gate strength `1` as a starting point. The controller writes a continuous night-visibility value derived from astronomical sun elevation, so the dome does not lose its night state when the physical sun light reaches zero intensity. Atmosphere Sun Light index `0` is still required for Sky Atmosphere rendering, but no longer controls this visibility gate.

### Daytime atmosphere appears at night or in wide-FOV corners

First confirm the old post-process material is no longer active. Then reduce **Sky Atmosphere Luminance Multiplier** or use an Atmosphere Fade By Sun Elevation curve that approaches zero at night.

### Clouds disappear when moon intensity is reduced

Confirm the sky dome is active and the legacy post-process path is inactive. The dome fixes cloud ordering, but volumetric clouds still require lighting. Keep the moon as Atmosphere Sun Light index `1`, use a modest moon intensity, and art-direct the cloud's **Cloud Scattering Luminance Scale** rather than making the whole night Directional Light excessively bright.

### The lights do not move

Set them to Movable, assign explicit references on the controller, and make sure the profile's Drive Sun Light and Drive Moon Light options are enabled. Remove any competing Blueprint Timeline.

### The moon mesh is missing

Confirm Moon Visual is enabled, a mesh is assigned, Uniform Scale is visible at the chosen Orbit Radius, and the mesh material renders unlit/emissive as intended. Use Facing Rotation Offset if a disc points along a different local axis.

### The clock unexpectedly resets

Stopping PIE creates a new GameInstance and intentionally resets the session. Within a play session, use normal level travel that preserves the GameInstance. A new process or SaveGame restore is outside Stage 1.

### A second controller reports an error

Remove the duplicate controller. The active-controller guard is deliberate so two actors cannot drive the same clock, lights, and presentation at once.
