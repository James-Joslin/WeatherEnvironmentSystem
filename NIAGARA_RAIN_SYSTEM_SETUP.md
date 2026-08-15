# Niagara Rain System Setup

This guide creates a production-oriented Niagara rain system for Unreal Engine 5.7 and connects it to `WeatherEnvironmentSystem` Stage 5. It assumes the level already contains one working `WeatherEnvironmentController`, a built weather grid, and a Stage 4 simulation profile.

The finished system will:

- loop continuously while a pooled rain component is assigned;
- receive intensity, transition, wind, cell centre, and cell extent from C++;
- spawn only around the active camera instead of filling a one-kilometre cell;
- reject particles that fall outside the owning weather cell;
- use separate near and far GPU sprite emitters;
- fade its spawn rate before the weather presenter recycles it.

## 1. Confirm the required plugins

1. Open **Edit > Plugins**.
2. Confirm **Niagara** is enabled.
3. Confirm **Weather Environment System** is enabled.
4. Restart Unreal Editor if either plugin was just enabled.
5. In the Content Browser settings, enable **Show Plugin Content** and **Show Engine Content**. This makes Niagara's default sprite material available during initial testing.

Create project-owned rain assets under a game-content folder such as:

```text
/Game/Weather/VFX/Rain
```

Keeping the authored effect in game content prevents a future plugin update from overwriting it. The weather profile can reference a Niagara system from any mounted content folder.

## 2. Create the Niagara system

1. Open `/Game/Weather/VFX/Rain` in the Content Browser.
2. Right-click empty space and choose **FX > Niagara System**.
3. Choose **New system from a template**.
4. Select **Simple Sprite Burst**. This is a standard/stateful emitter template.
5. Click **Create**.
6. Name the asset `NS_WeatherRain`.
7. Open it and rename its emitter `NE_WeatherRain_Near`.
8. Save the system.

Do not select **Minimal Lightweight** or another Lightweight/Stateless template for this walkthrough. A Lightweight emitter displays an orange `LW` badge, `[Lightweight]` in the preview statistics, and `Stateless` at the bottom of the viewport. It uses **Allowed Feature Mask > Execute GPU/Execute CPU** instead of **Sim Target**, and it cannot host the custom Scratch Pad module used later for camera-local, weather-cell-clipped spawning.

If `NS_WeatherRain` already contains the Lightweight emitter, you can preserve the system and its User Parameters:

1. Focus the Niagara editor and press **E** to open **Add Emitter**.
2. Select **Simple Sprite Burst** and add it.
3. Rename the new standard emitter `NE_WeatherRain_Near`.
4. Confirm its stack has separate **Emitter Spawn**, **Emitter Update**, **Particle Spawn**, and **Particle Update** groups.
5. Select its **Emitter Properties** and confirm **Sim Target** is visible.
6. Remove the old `LW` emitter only after the standard emitter is present.

Use the Niagara default material while building the behavior:

```text
/Niagara/DefaultAssets/DefaultSpriteMaterial
```

Replace it with the project's final unlit translucent rain-streak material after the integration works.

## 3. Create the exact user-parameter contract

Open the **User Parameters** tab in `NS_WeatherRain`. Press **+** for each parameter, choose the type shown below, and enter the name without manually typing the `User.` namespace. Niagara displays the completed name with that prefix.

The C++ presenter uses `SetVariableVec3` for vector values, so choose Niagara **Vector**/**Vector3**, not Niagara **Position**, for this version of the plugin.

| Name shown in Niagara | Type | Preview default |
| --- | --- | ---: |
| `User.WeatherCellCenter` | Vector3 | `(0, 0, 0)` |
| `User.WeatherCellExtent` | Vector3 | `(55000, 55000, 100000)` |
| `User.WeatherRainIntensity` | Float | `1.0` |
| `User.WeatherWindVector` | Vector3 | `(500, 0, 0)` |
| `User.WeatherSpawnRate` | Float | `6000.0` |
| `User.WeatherTransitionAlpha` | Float | `1.0` |

Parameter names and types must match exactly. `User.weatherspawnrate`, `Weather Spawn Rate`, and a Position-typed `WeatherCellCenter` are different parameters and will not receive these C++ overrides.

The preview values make the effect visible inside the Niagara editor. At runtime, the weather presenter overwrites them for every assigned cell.

## 4. Configure the near emitter lifecycle

Select `NE_WeatherRain_Near` in **System Overview**.

### Emitter Properties

These controls appear on the standard/stateful emitter created from **Simple Sprite Burst**. If the panel instead shows **Allowed Feature Mask**, return to the recovery steps in Section 2; that is a Lightweight/Stateless emitter rather than the emitter type required by this guide.

1. Set **Sim Target** to **GPU Compute Sim**.
2. Disable **Local Space**.
3. Set **Calculate Bounds Mode** to **Fixed**.
4. Use these initial local-space bounds for the default one-kilometre cell:

```text
Minimum: (-65000, -65000, -120000)
Maximum: ( 65000,  65000,  120000)
```

The pooled Niagara component is anchored at the weather-cell centre. Therefore, a GPU fixed bound must cover the complete cell half-extent, its overlap margin, and some wind travel even though actual particles spawn only near the camera.

If **System Properties > Fixed Bounds** is enabled, it overrides emitter bounds. Either disable the system override or give the system the same sufficiently large bounds.

### System State and Emitter State

1. Select **System State** and set **Loop Behavior** to **Infinite**.
2. Select the emitter's **Emitter State** module.
3. Set **Life Cycle Mode** to **Self** when that field is shown.
4. Set **Loop Behavior** to **Infinite**.
5. Disable loop delay.
6. Set **Inactive Response** to **Kill** or **Complete**. The presenter reaches transition alpha zero before calling immediate deactivation, so either is safe; **Kill** makes the pooling behavior easiest to inspect.

Do not use **Once**, a finite loop count, or a burst-only lifecycle. The rain presenter does not bind rain components to `OnSystemFinished`; a system that finishes itself can remain assigned but visually inactive.

## 5. Build the near-emitter stack

The near emitter should contain this stack:

```text
Emitter Update
    Emitter State
    Spawn Rate

Particle Spawn
    Initialize Particle
    Weather Rain Spawn              <- Scratch Pad module created below

Particle Update
    Particle State
    Solve Forces and Velocity

Render
    Sprite Renderer
```

Delete any **Spawn Burst Instantaneous** module inherited from a template. Rain density must come from a continuous **Spawn Rate** module.

### Configure Spawn Rate

1. Under **Emitter Update**, press **+** and add **Spawn Rate** if it is absent.
2. Open the value menu beside **Spawn Rate** and search dynamic inputs for **Multiply** or **Multiply Float**.
3. Build the following expression:

```text
User.WeatherSpawnRate
    * User.WeatherTransitionAlpha
    * 0.35
```

`0.35` is the near-layer fraction. If the current Niagara UI exposes only a two-input multiply, place another multiply dynamic input inside one input.

Do not multiply by `WeatherRainIntensity` again. C++ already calculates `WeatherSpawnRate` as the profile maximum multiplied by cell rain intensity.

### Configure Initialize Particle

Use these initial values:

| Setting | Near-emitter value |
| --- | --- |
| Lifetime | Random range `0.5–1.0` seconds |
| Sprite Size | Approximately `(2, 150)` cm |
| Color | White or slightly blue-grey |
| Alpha | `0.25–0.5` depending on the material |

If the rendered streak lies sideways after velocity alignment, swap the two Sprite Size components or rotate the authored streak texture by 90 degrees.

## 6. Create the camera-local spawn Scratch Pad module

This module uses Niagara Position values for camera/owner calculations, keeping large-world coordinate math relative to the component. `User.WeatherCellExtent` remains a Vector3 because it represents a size rather than an absolute position.

### Create the module

1. Press **+** beside **Particle Spawn**.
2. Select **New Scratch Pad Module**.
3. Rename it `WeatherRainSpawn`.
4. Open the Scratch Pad graph.
5. Ensure its **Module Usage Bitmask** includes **Particle Spawn**.

### Add module inputs

Create these `INPUT` values in the module's Map Get node:

| Input | Type | Near default |
| --- | --- | ---: |
| `INPUT.CameraQuery` | Camera Query data interface | Player Controller `0` |
| `INPUT.HorizontalRadius` | Float | `5000.0` |
| `INPUT.SpawnHeightMinimum` | Float | `3000.0` |
| `INPUT.SpawnHeightMaximum` | Float | `8000.0` |
| `INPUT.FallSpeedMinimum` | Float | `6000.0` |
| `INPUT.FallSpeedMaximum` | Float | `9000.0` |

Also add these existing parameters to Map Get:

```text
Engine.Owner.Position
User.WeatherCellExtent
User.WeatherWindVector
```

`Engine.Owner.Position` is the pooled Niagara component location and therefore the cell centre in Position form. The `User.WeatherCellCenter` contract value remains available to other modules, but the relative owner-position calculation is preferable for this spawn graph.

### Read the camera position

1. Drag from `INPUT.CameraQuery`.
2. Select **Get Camera Properties CPU/GPU**.
3. Use its **Camera Position World** output.
4. Leave **Player Controller Index** at `0` for a normal single-player view.

### Generate a random camera-local offset

Create three **Random Range Float** values that are evaluated per particle:

```text
RandomX = RandomRange(-HorizontalRadius, HorizontalRadius)
RandomY = RandomRange(-HorizontalRadius, HorizontalRadius)
RandomZ = RandomRange(SpawnHeightMinimum, SpawnHeightMaximum)
```

Combine them into:

```text
RandomOffset = MakeVector(RandomX, RandomY, RandomZ)
```

Now calculate:

```text
CameraOffsetFromCell = CameraPositionWorld - Engine.Owner.Position
CandidateOffsetFromCell = CameraOffsetFromCell + RandomOffset
CandidateWorldPosition = Engine.Owner.Position + CandidateOffsetFromCell
```

The first subtraction produces a Vector from two Position values. Adding that Vector back to `Engine.Owner.Position` produces the final Position without converting an absolute large-world position into a float Vector3.

### Reject positions outside the weather cell

Build these comparisons using `CandidateOffsetFromCell` and `User.WeatherCellExtent`:

```text
InsideX = Abs(CandidateOffsetFromCell.X) <= WeatherCellExtent.X
InsideY = Abs(CandidateOffsetFromCell.Y) <= WeatherCellExtent.Y
InsideCell = InsideX AND InsideY
```

Use a Map Set node to write:

```text
Particles.Position = CandidateWorldPosition
Particles.Alive = InsideCell
```

Rejecting outside particles preserves a clean rain-front boundary. Do not clamp outside positions to the cell edge, because that produces visible walls of concentrated rain.

### Set rain velocity

Create a per-particle fall speed:

```text
FallSpeed = RandomRange(FallSpeedMinimum, FallSpeedMaximum)
DownVelocity = (0, 0, -FallSpeed)
FinalVelocity = User.WeatherWindVector + DownVelocity
```

In the same Map Set node, write:

```text
Particles.Velocity = FinalVelocity
```

Connect the Parameter Map flow from the red input node, through every Map Get/Map Set section, to the green output node. Click **Apply and Save**.

Back in **System Overview**, select `Weather Rain Spawn` and confirm its near-emitter inputs are still:

```text
Horizontal Radius:       5000
Spawn Height Minimum:    3000
Spawn Height Maximum:    8000
Fall Speed Minimum:      6000
Fall Speed Maximum:      9000
```

## 7. Configure particle movement and rendering

### Particle Update

1. Retain **Particle State** so lifetime expiry kills particles.
2. Add **Solve Forces and Velocity** if the Minimal template did not include it.
3. Leave collision disabled for the first working version.

The spawn module sets initial velocity. **Solve Forces and Velocity** advances position each frame.

### Sprite Renderer

1. Select **Sprite Renderer**.
2. Assign `/Niagara/DefaultAssets/DefaultSpriteMaterial` initially.
3. Set **Facing Mode** to **Face Camera**.
4. Set **Alignment** to **Velocity Aligned**.
5. Ensure **Velocity Binding** uses `Particles.Velocity`.
6. Ensure **Sprite Size Binding** uses `Particles.SpriteSize`.
7. For a translucent final material, use **View Depth** sorting only if visual overlap requires it; sorting itself has a cost.

Compile and save. With the preview defaults, rain should now surround the Niagara preview camera and lean slightly in the positive X direction from the default wind.

## 8. Add the far-rain emitter

1. Duplicate `NE_WeatherRain_Near` inside `NS_WeatherRain`.
2. Rename the duplicate `NE_WeatherRain_Far`.
3. Keep **GPU Compute Sim**, world space, fixed bounds, and infinite looping.
4. Change its **Spawn Rate** layer fraction from `0.35` to `0.65`.
5. Use these far-layer values:

| Setting | Far-emitter value |
| --- | --- |
| Horizontal Radius | `10000` cm |
| Spawn Height Minimum | `6000` cm |
| Spawn Height Maximum | `12000` cm |
| Fall Speed Minimum | `4000` cm/s |
| Fall Speed Maximum | `7000` cm/s |
| Lifetime | `0.8–1.5` seconds |
| Sprite Size | Approximately `(1, 75)` cm |
| Alpha | Lower than the near layer |

6. Keep collision disabled on this emitter.
7. Compile and save the complete system.

The two layer fractions should normally total `1.0`. They divide the single cell spawn-rate budget rather than doubling it.

## 9. Assign the system to the weather profile

1. Open the `WeatherEnvironmentProfile` used by the level's `WeatherEnvironmentController`.
2. Expand **Presentation > Precipitation**.
3. Enable **Enabled**.
4. Assign `NS_WeatherRain` to **Rain System**.
5. Start with:

| Profile setting | Initial value |
| --- | ---: |
| Pool Size | `16` |
| Presentation Radius In Cells | `3.0` |
| Cell Overlap Fraction | `0.1` |
| Maximum Spawn Rate | `6000` |
| Fade In Seconds | `0.5` |
| Fade Out Seconds | `0.75` |
| Selection Update Interval Seconds | `0.1` |
| Intensity Priority Weight | `1.0` |
| Proximity Priority Weight | `1.0` |

6. Expand **Presentation > Niagara Parameters**.
7. Confirm every rain mapping still has its default exact name:

```text
Cell Center:       User.WeatherCellCenter
Cell Extent:       User.WeatherCellExtent
Rain Intensity:    User.WeatherRainIntensity
Wind Vector:       User.WeatherWindVector
Spawn Rate:        User.WeatherSpawnRate
Transition Alpha:  User.WeatherTransitionAlpha
```

8. Confirm that the controller's **Environment Profile** points to this edited profile.
9. Save the profile and level.

The controller creates Niagara components lazily. An empty level start with no nearby raining cells should show zero allocated rain components; this is expected.

## 10. Force a controlled rain test

Use a duplicate test profile so production front settings remain intact.

1. Duplicate the active weather profile and name it something like `DA_Weather_RainTest`.
2. Assign the duplicate to the level controller.
3. Under **Simulation > Front Lifecycle**, disable the lifecycle temporarily.
4. Clear **Initial Seeds** and set **Initial Generated Seed Count** to `0`.
5. Under **Simulation > Baseline Values**, set **Rain Intensity** to `0.8` or `1.0`.
6. Set **Minimum Weather Type Duration Seconds** to `0` for the test.
7. Keep the normal rain enter threshold at or below `0.55`.
8. Start PIE from a fresh session so initial Game Instance state is rebuilt.
9. Select the runtime `WeatherEnvironmentController` and inspect its `PrecipitationPresenter`:

```text
Allocated Component Count > 0
Active Cell Count          > 0
```

10. Move across a cell boundary and verify that the visible volume stays around the camera while the cell clipping boundary changes cleanly.
11. Stop PIE and restore the production environment profile after validation.

You can also enable the controller's weather-grid debug labels and **Draw Rain Intensity** to verify that the CPU cell is authoritatively raining before diagnosing Niagara.

## 11. Add optional polish only after integration works

### Rain material

Replace the default sprite material with an unlit translucent material that:

- uses a narrow vertical streak texture or analytic soft streak mask;
- multiplies opacity by `Particle Color.A`;
- uses a subdued blue-grey tint;
- disables unnecessary lighting and expensive refraction;
- uses depth fade sparingly near geometry.

### Near-layer collision and splashes

If splashes are important:

1. Add scene-depth or distance-field collision only to the near emitter.
2. Keep far rain collision-free.
3. Prefer a separate low-rate splash emitter or dedicated ground effect.
4. Avoid producing a splash event for every rain particle.

### Intensity-dependent appearance

`User.WeatherRainIntensity` is already available independently of spawn rate. Use it to drive opacity, streak length, mist, turbulence, or splash probability. Do not use it as a second density multiplier unless that nonlinear response is intentional.

## 12. Production checks

Before considering the effect finished, verify all of the following:

- Both emitters and the system loop infinitely.
- There is no burst-only spawn module.
- `Local Space` is disabled.
- GPU emitters have valid fixed bounds covering one complete weather cell plus margin.
- The camera-local Scratch Pad module uses `Engine.Owner.Position` for relative Position math.
- Outside-cell samples set `Particles.Alive` false instead of clamping to an edge.
- The near/far spawn-rate fractions total approximately one.
- `WeatherTransitionAlpha` multiplies spawn rate.
- A clear cell has no assigned active rain effect.
- Rain intensity changes do not restart the Niagara component.
- Leaving rain fades emission before the component deactivates.
- Moving between adjacent raining cells does not reveal a gap.
- Niagara distance culling is disabled initially, or its range exceeds the cell half-diagonal plus the far spawn radius.
- The final maximum spawn rate is tested with several neighboring pooled systems active, not only in the Niagara asset preview.

## Troubleshooting

### The Niagara preview works, but PIE has no rain

- Confirm `NS_WeatherRain` is assigned under **Presentation > Precipitation > Rain System**.
- Confirm the cell's authoritative `bIsRaining` value is true; intensity alone does not activate the presenter.
- Confirm all user-parameter names and types match exactly.
- Confirm the controller has the edited profile and is the only weather controller in the level.
- Confirm the camera lies inside or near the built weather grid.

### Rain appears near the cell centre instead of the camera

- Confirm the Scratch Pad module calls **Get Camera Properties CPU/GPU**.
- Confirm `Particles.Position` receives `Engine.Owner.Position + CandidateOffsetFromCell`.
- Confirm the emitter is not in Local Space.

### Rain disappears near a cell edge

- Increase fixed bounds to cover the complete cell and wind margin.
- Confirm **System Properties > Fixed Bounds** is not overriding the emitter with smaller bounds.
- Keep the profile's **Cell Overlap Fraction** at approximately `0.1` while testing.

### Rain forms a dense wall at a boundary

- Reject outside-cell positions by setting `Particles.Alive` false.
- Do not clamp candidate positions to the cell minimum or maximum.

### Rain is sideways

- Confirm Sprite Renderer **Alignment** is **Velocity Aligned**.
- Swap the X/Y components of `Particles.SpriteSize`, or rotate the streak texture.

### Rain is too expensive

- Reduce **Maximum Spawn Rate** before reducing the weather-system pool.
- Reduce far-layer alpha and radius.
- Keep far collision disabled.
- Inspect translucent overdraw and GPU particle time with multiple raining cells active.
- Add Niagara Effect Type scalability only after the uncapped effect behaves correctly.

## Official Niagara references

- [Niagara overview for Unreal Engine 5.7](https://dev.epicgames.com/documentation/unreal-engine/overview-of-niagara-effects-for-unreal-engine?application_version=5.7)
- [Creating a GPU sprite effect](https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-create-a-gpu-sprite-effect-in-niagara-for-unreal-engine)
- [Niagara Scratch Pad modules](https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-scratch-pad-modules-in-unreal-engine)
- [Niagara render-module reference](https://dev.epicgames.com/documentation/unreal-engine/render-module-reference-for-niagara-effects-in-unreal-engine)
