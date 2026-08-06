# Weather Environment System

Weather Environment System is a standalone Unreal Engine 5.7 plugin for the game's world clock, astronomical day/night cycle, moon visual, stylised sky, landscape weather grid, and wind-driven foliage. It remains separate from `OceanSystemPlugin` and `PCGExtensionsFeoul`.

## Current implementation

Version 0.4.3 currently provides Stages 1–4:

- a persistent session clock owned by `WeatherStateSubsystem`;
- configurable time scale, pause, date, and time controls;
- astronomical sun and moon directions;
- runtime sun and moon Directional Light control;
- sunrise and sunset events;
- an orbiting moon static mesh;
- a camera-centred Opaque/Unlit/Is Sky dome;
- four independently graded sky cubemaps;
- Blueprint-readable time and astronomy values.
- a deterministic, landscape-spanning CPU weather grid with constant-time point queries;
- editor/PIE grid, influence-sphere, and exact wind-vector debug drawing;
- HUD-independent per-cell weather labels rendered directly by an editor component visualizer;
- a movable wind director with manual, once, loop, and ping-pong route behavior;
- fixed-step direction, speed, curl, gust, dead-zone, and exponential smoothing logic;
- a shared wind field texture plus player-local MPC fallbacks;
- `MF_WeatherFoliageWind` for separate trunk/branch sway and leaf rustle without per-instance MID updates.
- deterministic fixed or profile-sampled weather seeds with stable IDs and finite lifetimes;
- wind-advection with wrap, clamp, or expire boundary behavior;
- normalized three-sigma Gaussian fronts evaluated only over affected cell neighborhoods;
- double-buffered, bilinearly and temporally interpolated continuous weather queries;
- rain/storm threshold hysteresis and minimum-duration weather classification;
- ordered, priority-based `WeatherTypeLookupDataAsset` classification rules.
- an area-scaled deterministic front lifecycle with weighted weather archetypes, minimum spacing, finite lifetimes, and upwind replenishment;
- Blueprint controls for inspecting the target front count and forcing an immediate or gradual replenishment pass.

Precipitation, local volumetric clouds, and Ocean System integration are later milestones. See the repository-level implementation plan for the complete staged plan.

## Initial Unreal setup

1. Enable **Weather Environment System** under **Edit > Plugins** and restart Unreal Editor when requested.
2. Enable **Show Plugin Content** in the Content Browser.
3. Confirm `/WeatherEnvironmentSystem/Materials/M_WeatherSkyDome` exists.
4. In the game Content folder, create **Miscellaneous > Data Asset > WeatherEnvironmentProfile**.
5. Name the profile something such as `DA_MainWorld_WeatherEnvironment`.
6. Place exactly one **Weather Environment Controller** in the open-world level.
7. Assign the profile to the controller's **Environment Profile** property.
8. Assign the sun, moon, and Skylight references directly, or use the controller's automatic light discovery.

The recommended level contains:

- a Movable sun Directional Light using Atmosphere Sun Light index `0`;
- a Movable moon Directional Light using Atmosphere Sun Light index `1`;
- one Sky Atmosphere;
- one Volumetric Cloud when clouds are required;
- one Skylight, preferably using Real Time Capture;
- one Weather Environment Controller;
- the game's existing Post Process Volume for exposure, colour grading, bloom, and depth of field.

Do not install the weather sky in the Post Process Volume. Remove the old stars/weather-sky blendable, enable **Skybox**, and leave **Use Legacy Post Process Sky** disabled in the profile. The controller owns the sky dome and its runtime MID.

For the complete level and material instructions, see:

- [IN_ENGINE_SETUP_GUIDE.md](IN_ENGINE_SETUP_GUIDE.md)
- [SKY_DOME_SETUP.md](SKY_DOME_SETUP.md)
- [CUSTOM_SKYBOX_MATERIAL_SETUP.md](CUSTOM_SKYBOX_MATERIAL_SETUP.md) — legacy fallback only
- [WIND_FOLIAGE_SETUP.md](WIND_FOLIAGE_SETUP.md)
- [WEATHER_SIMULATION_SETUP.md](WEATHER_SIMULATION_SETUP.md)

## Blueprint clock and Widget UI

The authoritative clock lives in `WeatherStateSubsystem`, which is a Game Instance Subsystem. A Widget Blueprint should display that state; it should not maintain or advance a separate clock.

### Create the widget

1. Create or open the HUD Widget Blueprint, for example `WBP_MainHUD`.
2. Add a Text Block and name it `ClockText`.
3. Have the Player Controller or HUD create `WBP_MainHUD` and add it to the viewport at Begin Play.

The intended ownership is:

```text
Player Controller or HUD
    -> creates WBP_MainHUD
        -> reads WeatherStateSubsystem
```

### Display hours and minutes

In the Widget Blueprint's **Event Construct**:

1. Use **Get Game Instance Subsystem** with class `WeatherStateSubsystem`.
2. Store the returned subsystem in a widget variable if it will be used repeatedly.
3. Use **Bind Event to On Minute Changed**.
4. Connect a custom event such as `Handle Minute Changed`. Its `DateTime` input may be retained even when the event only calls the refresh function.
5. Call a `Refresh Clock` function immediately so the Text Block is populated before the next minute boundary.

Implement `Refresh Clock` as:

```text
WeatherStateSubsystem
    -> Get Clock Display String (Include Seconds = false)
    -> convert String to Text if required
    -> Set Text on ClockText
```

This event-driven approach is preferred for an `HH:MM` display because it avoids updating the widget every frame.

In **Event Destruct**, unbind the widget's specific `Handle Minute Changed` event from `On Minute Changed`. Do not use **Unbind All Events**, because other systems may also listen to the subsystem.

### Display seconds

For `HH:MM:SS`, call `Get Clock Display String` with **Include Seconds** enabled. Use a looping timer to call `Refresh Clock` and retain its Timer Handle so it can be cleared in **Event Destruct**.

- A one-real-second interval is sufficient for a near-real-time clock.
- Use a shorter interval such as `0.1`–`0.25` seconds when the game clock is heavily accelerated and the displayed seconds must look responsive.
- Avoid Widget Tick unless the design genuinely needs a per-frame clock display.

The minute event can remain bound alongside the timer so minute-boundary UI logic has an exact event.

### Available clock Blueprint functions

`WeatherStateSubsystem` exposes:

- `Get Current Date Time`;
- `Get Normalized Day Fraction`;
- `Get Clock Display String`;
- `Set Date Time`;
- `Advance World Seconds`;
- `Set Time Scale` and `Get Time Scale`;
- `Set Clock Paused` and `Is Clock Paused`;
- `On Minute Changed`, `On Hour Changed`, and `On Day Changed`.

The controller additionally exposes:

- `Get Sun Direction` and `Get Sun Elevation Degrees`;
- `Get Moon Direction` and `Get Moon Phase`;
- `Get Skybox Material Instance`;
- `On Sunrise` and `On Sunset`;
- `Refresh Environment`.

The subsystem survives normal level travel while the same Game Instance remains alive. It intentionally resets when PIE is stopped or the application is restarted. SaveGame persistence is not part of Stage 1.

## Sky material maintenance

The supplied sky-dome material is generated and repaired with:

```text
Weather.GenerateSkyDomeMaterial
```

After changing plugin shader source while the editor is already open, force the loaded dome material to compile with:

```text
RecompileShaders Material M_WeatherSkyDome
```

The legacy post-process generator remains available as `Weather.GenerateSkyboxMaterial`, but it should not be active at the same time as the sky dome.

## Source modules

- `WeatherEnvironmentSystem`: runtime clock, astronomy, controller, profile, and presentation code.
- `WeatherEnvironmentSystemEditor`: editor-only material generation and repair commands.

The runtime module may consume Ocean System APIs in later stages. Dependency direction remains one-way: Ocean System must not depend on Weather Environment System.

## Automated tests

Stage tests are registered under:

```text
WeatherEnvironment.Stage1
WeatherEnvironment.Stage2
WeatherEnvironment.Stage3
WeatherEnvironment.Stage4
```

Open **Tools > Test Automation**, filter for `WeatherEnvironment`, and run the clock, astronomy, grid, wind-route, field-mapping, Gaussian propagation, advection, determinism, and classification tests.
