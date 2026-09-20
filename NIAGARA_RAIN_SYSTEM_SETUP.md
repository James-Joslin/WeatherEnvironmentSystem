# Niagara Rain System Setup

This guide creates a production-oriented Niagara rain system for Unreal Engine 5.7.3 and connects it to `WeatherEnvironmentSystem` Stage 5. It assumes the level already contains one working `WeatherEnvironmentController`, a built weather grid, and a Stage 4 simulation profile.

The finished system will:

- loop continuously while a pooled rain component is assigned;
- receive intensity, transition, wind, cell centre, and cell extent from C++;
- spawn only around the active camera instead of filling a one-kilometre cell;
- reject particles that fall outside the owning weather cell;
- use separate near and far GPU sprite emitters;
- fade its spawn rate before the weather presenter recycles it.

### Niagara names used in this guide

Niagara displays namespaces as coloured badges rather than always printing the dot-qualified name. These are the same parameter:

```text
Editor display:  [USER] WeatherCellExtent
C++ name:        User.WeatherCellExtent
```

This guide uses the full C++ form when describing the runtime contract. A Scratch Pad module does not need to read the `USER` namespace directly. Create an `INPUT` pin inside the module, then bind that input to the corresponding `USER` parameter on the module's stack entry after applying the graph. The `USER` parameter is searched for in that stack-input picker, not in the Scratch Pad's Map Get picker.

The three kinds of values used below are different:

| Editor badge | Full form used in this guide | Where it comes from |
| --- | --- | --- |
| `USER` | `User.WeatherCellExtent` | Created once on the Niagara system and overwritten by the weather presenter at runtime |
| `INPUT` | `Input.WeatherCellExtent` | Created inside the Scratch Pad module, then configured or bound on that module's stack entry |
| `ENGINE OWNER` | `Engine.Owner.Position` | Built into Niagara; do not create it |

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

These parameters are project-specific and will not exist in Niagara's search results until you create them.

1. Return to the `NS_WeatherRain` **System Overview** tab. Do not stay inside the Scratch Pad graph for this step.
2. Under the preview viewport, select **User Parameters**, not the similarly named **Parameters** tab.
3. Under **User Exposed**, click **+**.
4. Choose **Vector** for the first parameter.
5. Name it `WeatherCellCenter` and press Enter. Do not type `User.` into this name field; the `USER` badge supplies that namespace.
6. Repeat the process for the remaining five parameters, selecting **Vector** or **Float** from the table below.

The final rows in the editor should display a `USER` badge followed by names such as `WeatherCellExtent`. In C++ and the weather profile, the same name is written as `User.WeatherCellExtent`.

The C++ presenter uses `SetVariableVec3` for vector values, so choose Niagara **Vector**/**Vector3**, not Niagara **Position**, for this version of the plugin.

| Name beside the `USER` badge | Full runtime name | Type | Preview value |
| --- | --- | --- | ---: |
| `WeatherCellCenter` | `User.WeatherCellCenter` | Vector3 | `(0, 0, 0)` |
| `WeatherCellExtent` | `User.WeatherCellExtent` | Vector3 | `(60000, 60000, 100000)` |
| `WeatherRainIntensity` | `User.WeatherRainIntensity` | Float | `1.0` |
| `WeatherWindVector` | `User.WeatherWindVector` | Vector3 | `(500, 0, 0)` |
| `WeatherSpawnRate` | `User.WeatherSpawnRate` | Float | `6000.0` |
| `WeatherTransitionAlpha` | `User.WeatherTransitionAlpha` | Float | `1.0` |

### Set the preview values

The general **Parameters** tab is a reference browser, so values shown there are intentionally read-only. To edit the Niagara system's defaults:

1. Return to **System Overview**.
2. Click **User Parameters** on the blue `NS_WeatherRain` system node. In some layouts, editing directly in the **User Parameters** tab below the viewport provides the same controls.
3. Enter the preview values from the table in the Details panel.
4. Compile and save the Niagara system.

If a field is greyed out, confirm that you selected the blue system's **User Parameters** row and not the general **Parameters** tab or a parameter reference inside the Scratch Pad graph.

The `60000` cm XY value matches the current runtime calculation:

```text
100000 * (0.5 + 0.1) = 60000 cm half-extent
```

Here `100000` is the default cell size and `0.1` is the profile's default **Cell Overlap Fraction**. The presenter replaces this preview value with the actual grid-derived extent during play.

Parameter names and types must match exactly. `User.weatherspawnrate`, `Weather Spawn Rate`, and a Position-typed `WeatherCellCenter` are different parameters and will not receive these C++ overrides.

The preview values only make the effect visible inside the Niagara editor. At runtime, the weather presenter overwrites them for every assigned cell.

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
3. In the Multiply input, search for `WeatherSpawnRate` and select the entry carrying the `USER` badge. Do not choose **Read from new User parameter**, because the parameter already exists.
4. Add `WeatherTransitionAlpha` in the same way.
5. Build the following expression:

```text
User.WeatherSpawnRate
    * User.WeatherTransitionAlpha
    * 0.35
```

`0.35` is the near-layer fraction. If the current Niagara UI exposes only a two-input Multiply, put a second Multiply inside one of its inputs.

If neither weather value appears in this picker, return to Section 3 and create the system User Parameters first. Search by the suffix (`WeatherSpawnRate`), not by the displayed phrase `User Weather Spawn Rate`.

Do not multiply by `WeatherRainIntensity` again. C++ already calculates `WeatherSpawnRate` as the profile maximum multiplied by cell rain intensity.

### Configure Initialize Particle

Select **Initialize Particle** in the Particle Spawn group. Some value fields remain grey until their corresponding mode is enabled. Configure the modes first:

1. Under **Point Attributes**, set **Lifetime Mode** to **Random**. This reveals the Minimum and Maximum fields.
2. Set lifetime Minimum to `0.5` and Maximum to `1.0` seconds.
3. Set **Color Mode** to **Direct Set**, then choose white or a slightly blue-grey colour with alpha between `0.25` and `0.5`.
4. Under **Sprite Attributes**, enable **Sprite Size** if it has a separate checkbox.
5. Set **Sprite Size Mode** to **Non-Uniform** so separate X and Y fields become editable.
6. Set Sprite Size to approximately `X = 2`, `Y = 150` cm.

The resulting starting values are:

| Setting | Near-emitter value |
| --- | --- |
| Lifetime | Random range `0.5–1.0` seconds |
| Sprite Size | Approximately `(2, 150)` cm |
| Color | White or slightly blue-grey |
| Alpha | `0.25–0.5` depending on the material |

If the rendered streak lies sideways after velocity alignment, swap the two Sprite Size components or rotate the authored streak texture by 90 degrees.

## 6. Create the camera-local spawn Scratch Pad module

This module uses Niagara Position values for camera/owner calculations, keeping large-world coordinate math relative to the component. Its `Input.WeatherCellExtent` remains a Vector3 because it represents a size rather than an absolute position; the stack later binds it to `User.WeatherCellExtent`.

### Create the module

1. Press **+** beside **Particle Spawn**.
2. Select **New Scratch Pad Module**.
3. Rename it `WeatherRainSpawn`.
4. Open the Scratch Pad graph.
5. Click empty graph background so no node is selected.
6. In the Details panel, open **Module Usage Bitmask**.
7. Leave **Module** enabled and ensure **Particle Spawn Script** is enabled. `Particle Spawn Script` is the complete UE 5.7.3 label; there is no separate option named only `Particle Spawn`.

Because the Scratch Pad was created from the Particle Spawn group's **+** menu, **Particle Spawn Script** will normally already be enabled. This setting controls where the module may be placed; it does not create particles by itself.

### Add module inputs

The new graph contains a red **Input Map**, a **Map Get**, a **Map Set**, and a green **Output Module**. The white Parameter Map wire must eventually run from the red node through the Map Set to the green node.

Create these module inputs on the existing **Map Get** node:

| Name beside the `INPUT` badge | Type | Value to set later on the module stack |
| --- | --- | ---: |
| `CameraQuery` | Camera Query data interface | Player Controller Index `0` |
| `HorizontalRadius` | Float | `5000.0` |
| `SpawnHeightMinimum` | Float | `3000.0` |
| `SpawnHeightMaximum` | Float | `8000.0` |
| `FallSpeedMinimum` | Float | `6000.0` |
| `FallSpeedMaximum` | Float | `9000.0` |
| `WeatherCellExtent` | Vector | Bind to `[USER] WeatherCellExtent` |
| `WeatherWindVector` | Vector | Bind to `[USER] WeatherWindVector` |

For each Float input:

1. Click the **+** pin on **Map Get**.
2. Search for `Float` and choose the entry in the `INPUT` namespace.
3. Rename the new input with F2 or its context menu, using the name from the table without typing `INPUT.`.

For each Vector input:

1. Click the **+** pin on **Map Get**.
2. Search for `Vector` and choose the yellow Vector entry in the `INPUT` namespace.
3. Rename it `WeatherCellExtent` or `WeatherWindVector`, without typing `INPUT.` or `USER.`.

For `CameraQuery`, click the Map Get **+**, search for `Camera Query`, and choose the Camera Query data-interface entry in the `INPUT` namespace.

Do not try to enter the table's values on the Map Get node. Those graph-pin defaults can appear grey and are not the module-instance controls used by this walkthrough. After the graph is complete, click **Apply and Save**, return to System Overview, select the `Weather Rain Spawn` stack entry, enter the five numeric values there, and bind the two Vector inputs to the existing User Parameters.

Adding a pin to Map Get is not enough to expose it on the stack. Niagara must see an actual read of that pin in the connected module graph. Each input above must feed the position, clipping, or velocity calculations described below, and those results must reach the Map Set connected to **Output Module**. If an input is still unused, its read count is `0` and Niagara can omit it from the module stack.

Also add this existing engine parameter to Map Get:

```text
Engine.Owner.Position
```

Click the Map Get **+**, search for `Owner Position`, and select the purple `ENGINE OWNER` Position result. This parameter is built into Niagara.

It is expected that `WeatherCellExtent`, `WeatherWindVector`, and the `USER` namespace may not appear in this Map Get picker. They belong to the Niagara system's exposed parameter store, whereas this graph is declaring the reusable inputs of one module. The connection between the two is made on the module stack after **Apply and Save**.

At this point the Map Get should contain:

```text
[INPUT]        CameraQuery
[INPUT]        HorizontalRadius
[INPUT]        SpawnHeightMinimum
[INPUT]        SpawnHeightMaximum
[INPUT]        FallSpeedMinimum
[INPUT]        FallSpeedMaximum
[INPUT]        WeatherCellExtent
[INPUT]        WeatherWindVector
[ENGINE OWNER] Position
```

`Engine.Owner.Position` is the pooled Niagara component location and therefore the cell centre in Position form. The `User.WeatherCellCenter` contract value remains available to other modules, but the relative owner-position calculation is preferable for this spawn graph.

### Read the camera position

1. Drag from `INPUT.CameraQuery`.
2. Select **Get Camera Properties CPU/GPU**.
3. Use its **Camera Position World** output.
4. Leave **Player Controller Index** at `0` for a normal single-player view. This setting is inside the Camera Query data-interface input; it is not another User Parameter.

### Generate a random camera-local offset

Create three **Random Range Float** values that are evaluated per particle:

```text
RandomX = RandomRange(-HorizontalRadius, HorizontalRadius)
RandomY = RandomRange(-HorizontalRadius, HorizontalRadius)
RandomZ = RandomRange(SpawnHeightMinimum, SpawnHeightMaximum)
```

In the graph, right-click empty space and add **Random Range Float** three times. Connect the appropriate `INPUT` float pins to their Minimum and Maximum inputs. For the X and Y minimum values, pass `HorizontalRadius` through a **Negate** node so the range is `-HorizontalRadius` to `+HorizontalRadius`.

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

Build this with one **Subtract** node followed by two **Add** nodes. Niagara's numeric nodes adapt their types from their connections:

1. Connect the two purple Position values to **Subtract**; its result becomes a yellow Vector.
2. Add `RandomOffset` to that Vector to produce `CandidateOffsetFromCell`.
3. Add `CandidateOffsetFromCell` to the purple owner Position to produce a purple `CandidateWorldPosition`.

Do not use a generic Vector3 conversion on either absolute Position. Preserving Position types here avoids large-world-coordinate precision loss.

The first subtraction produces a Vector from two Position values. Adding that Vector back to `Engine.Owner.Position` produces the final Position without converting an absolute large-world position into a float Vector3.

### Reject positions outside the weather cell

Build these comparisons using `CandidateOffsetFromCell` and `Input.WeatherCellExtent`:

```text
InsideX = Abs(CandidateOffsetFromCell.X) <= WeatherCellExtent.X
InsideY = Abs(CandidateOffsetFromCell.Y) <= WeatherCellExtent.Y
InsideCell = InsideX AND InsideY
```

`InsideX`, `InsideY`, and `InsideCell` are explanatory names for values carried by wires; do not create Niagara parameters with those names. In the finished graph, the two red comparison outputs are the first two values and the red **Result** output of **Logic AND** is `InsideCell`. Comment boxes may be used to label these sections visually.

Niagara Vector pins do not use Blueprint's normal **Split Struct Pin** workflow. Extract each component with **Select V3 Channel**, which returns one Float from a Vector:

1. Drag from the yellow `CandidateOffsetFromCell` output and search for **Select V3 Channel**. Add it and set **Target Channel** to **X**.
2. Add another **Select V3 Channel** from the same vector and set its channel to **Y**.
3. Drag from `[INPUT] WeatherCellExtent`, add two more **Select V3 Channel** nodes, and set their channels to **X** and **Y** respectively.
4. If the node is hard to find, search for `SelectV3Channel`, `component mask`, or `Returns a single channel from a vector`.
5. Pass the candidate X and Y Float outputs through separate **Absolute Value** nodes.
6. Add two **Less Than or Equal** comparisons: absolute candidate X against extent X, and absolute candidate Y against extent Y.
7. Combine the two Boolean results with **Logic AND**.

The blue pins on generic Add and Subtract nodes are Niagara **Numeric** wildcard pins. Connecting the `CandidateOffsetFromCell` output to **Select V3 Channel** constrains that value to a Vector; the selector's result is a green Float suitable for **Absolute Value** and **Less Than or Equal**.

Use a Map Set node to write:

```text
Particles.Position = CandidateWorldPosition
DataInstance.Alive = InsideCell
```

For the Boolean output, click Map Set **+**, search for `Alive`, and select **Data Instance > Alive** (displayed as `[DATA INSTANCE] Alive`). UE 5.7 does not expose this kill flag as `Particles.Alive`. Connect the red **Result** pin of **Logic AND** directly to the red `DataInstance.Alive` input: true keeps an inside particle and false rejects an outside particle.

Rejecting outside particles preserves a clean rain-front boundary. Do not clamp outside positions to the cell edge, because that produces visible walls of concentrated rain.

### Set rain velocity

Create a per-particle fall speed:

```text
FallSpeed = RandomRange(FallSpeedMinimum, FallSpeedMaximum)
DownVelocity = (0, 0, -FallSpeed)
FinalVelocity = Input.WeatherWindVector + DownVelocity
```

Use **Random Range Float** for `FallSpeed`, **Negate** it, place that negative result in the Z input of **Make Vector**, and then use **Add** to combine it with the yellow `INPUT WeatherWindVector` pin.

In the same Map Set node, write:

```text
Particles.Velocity = FinalVelocity
```

Connect the Parameter Map flow from the red input node, through every Map Get/Map Set section, to the green output node. Click **Apply and Save**.

Back in **System Overview**, select the `Weather Rain Spawn` stack entry and bind its two weather inputs:

1. Open the value/dropdown menu beside **Weather Cell Extent**.
2. Search for `WeatherCellExtent` and select the existing `User.WeatherCellExtent` entry under **Link Inputs > User**. Depending on panel width, Niagara may show this as a `USER` badge followed by `WeatherCellExtent`.
3. Open the menu beside **Weather Wind Vector**.
4. Search for `WeatherWindVector` and select the existing `User.WeatherWindVector` entry under **Link Inputs > User**.

Choose the existing linked inputs. Do not choose **Read from new User parameter** because the required parameters already exist and that command can create a second uniquely named parameter.

Do not preview the clipping result while **Weather Cell Extent** is still an unbound local Vector. Its initial value may be `(0,0,0)`, which makes nearly every candidate fail the X/Y comparisons. After binding, expand the linked value if necessary and confirm the system User Parameter supplies approximately `(60000,60000,100000)` for the default cell.

On the same stack entry, enter these near-emitter module-instance values. This is the correct place to set the defaults that may have appeared grey in the Scratch Pad graph:

```text
Horizontal Radius:       5000
Spawn Height Minimum:    3000
Spawn Height Maximum:    8000
Fall Speed Minimum:      6000
Fall Speed Maximum:      9000
```

Expand `CameraQuery` on the same stack entry and leave **Player Controller Index** at `0`. If split-screen support is added later, the current plugin will need an explicit per-local-player camera selection contract rather than changing this value globally.

## 7. Configure particle movement and rendering

### Particle Update

1. Retain **Particle State** so lifetime expiry kills particles.
2. Add **Solve Forces and Velocity** if the Simple Sprite Burst template did not include it.
3. Leave collision disabled for the first working version.

The Scratch Pad module already writes a strong negative Z velocity, so a Gravity module is not required. **Solve Forces and Velocity** advances position each frame using that velocity. If you later want accelerating drops, add **Gravity Force** immediately above **Solve Forces and Velocity**, but keep the initial downward velocity.

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
- Outside-cell samples set `DataInstance.Alive` false instead of clamping to an edge.
- The near/far spawn-rate fractions total approximately one.
- `WeatherTransitionAlpha` multiplies spawn rate.
- A clear cell has no assigned active rain effect.
- Rain intensity changes do not restart the Niagara component.
- Leaving rain fades emission before the component deactivates.
- Moving between adjacent raining cells does not reveal a gap.
- Niagara distance culling is disabled initially, or its range exceeds the cell half-diagonal plus the far spawn radius.
- The final maximum spawn rate is tested with several neighboring pooled systems active, not only in the Niagara asset preview.

## Troubleshooting

### Connecting `DataInstance.Alive` leaves very few or no rain particles

- This means the boundary test is returning false for most spawn candidates; forcing Alive on merely bypasses that test.
- On the `Weather Rain Spawn` stack entry, confirm **Weather Cell Extent** is linked to `[USER] WeatherCellExtent`, rather than using a local zero Vector.
- Under the system **User Parameters**, confirm the preview value is approximately `(60000,60000,100000)` for the default cell.
- For a quick binding test, temporarily give the module input a local `(60000,60000,100000)` value. If dense rain returns, the comparison graph is working and the User Parameter link was the problem; restore the link afterwards.
- Confirm the selector pairing is candidate X against extent X and candidate Y against extent Y. Only the candidate components pass through **Absolute Value**.
- If the binding and wiring are correct, the preview camera may be outside the test cell centred on `Engine.Owner.Position`. Move the preview camera closer to the system origin or temporarily increase the extent to verify this case.

### `Weather Rain Spawn` expands but its body is completely empty

- Reopen the Scratch Pad by double-clicking the module or clicking its pencil icon.
- Confirm the white Parameter Map path reaches **Output Module**: **Input Map -> Map Set -> Output Module**.
- Confirm every `INPUT` output pin is connected to a calculation that ultimately writes `Particles.Position`, `DataInstance.Alive`, or `Particles.Velocity`. Merely listing inputs on Map Get does not expose unused reads.
- As a quick proof, connect `Input.WeatherWindVector` directly to `Particles.Velocity` on Map Set, click **Apply and Save** in the Scratch Pad toolbar, and return to System Overview. **Weather Wind Vector** should then appear in the expanded module.
- Use **Apply** or **Apply and Save** from the Scratch Pad toolbar; saving only the Niagara system does not commit graph changes to its stack entries.
- Once the inputs appear, complete the calculations and bind the two weather inputs through **Link Inputs > User**.

### `WeatherCellExtent` or `WeatherWindVector` is missing from search

- If you are searching from a Scratch Pad **Map Get**, this is expected. Do not look for the `USER` namespace there.
- Add two Vector inputs in the `INPUT` namespace and name them `WeatherCellExtent` and `WeatherWindVector`.
- Click **Apply and Save**, return to **System Overview**, and select the `Weather Rain Spawn` stack entry.
- Use each input's dropdown to select **Link Inputs > User > User.WeatherCellExtent** or **User.WeatherWindVector**.
- If the existing User Parameter is missing from the stack-input picker too, confirm it was created under **System Overview > User Parameters** with the exact Vector type, then compile the system.

### Parameter values or Scratch Pad defaults are greyed out

- The general **Parameters** tab is a read-only parameter/reference browser; use **User Parameters** to edit system User Parameter preview values.
- Scratch Pad Map Get pins define inputs but are not the clearest place to configure this module instance.
- Click **Apply and Save**, return to System Overview, select the `Weather Rain Spawn` stack entry, and enter Horizontal Radius, spawn height, and fall-speed values there.
- In **Initialize Particle**, choose modes such as **Lifetime Mode: Random**, **Color Mode: Direct Set**, and **Sprite Size Mode: Non-Uniform** before editing their dependent fields.

### Module Usage Bitmask has no option named `Particle Spawn`

- In Unreal Engine 5.7.3 the option is named **Particle Spawn Script**.
- Leave **Module** enabled and enable **Particle Spawn Script**.
- If the Scratch Pad module was added from the Particle Spawn group's **+** button, this usage should already be enabled.

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

- Reject outside-cell positions by setting `DataInstance.Alive` false.
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
- [Creating a GPU sprite effect](https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-create-a-gpu-sprite-effect-in-niagara-for-unreal-engine?application_version=5.7)
- [Niagara Scratch Pad modules for Unreal Engine 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-scratch-pad-modules-in-unreal-engine?application_version=5.7)
- [Niagara render-module reference](https://dev.epicgames.com/documentation/unreal-engine/render-module-reference-for-niagara-effects-in-unreal-engine?application_version=5.7)
