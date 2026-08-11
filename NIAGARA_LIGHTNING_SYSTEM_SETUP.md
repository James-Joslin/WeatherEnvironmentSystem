# Niagara Lightning System Setup

This guide creates a one-shot Niagara lightning system for Unreal Engine 5.7 and connects it to `WeatherEnvironmentSystem` Stage 5. It assumes the level already contains one working `WeatherEnvironmentController`, a built weather grid, and a Stage 4 simulation profile.

The finished system will:

- use Niagara's Static Beam and Ribbon Renderer workflow;
- start at the Niagara component placed high above the strike;
- terminate at the terrain/water location supplied by the weather presenter;
- scale width and brightness from normalized lightning intensity;
- flicker briefly and then finish cleanly;
- notify the presenter through `OnSystemFinished`, allowing its bounded active-effect slot to be released;
- expose separate Blueprint hooks for the light flash, camera response, and delayed thunder.

Rain and lightning use opposite lifecycles. Rain loops indefinitely until its pooled component is deactivated. Lightning must be finite and non-looping.

## 1. Confirm the required plugins and folders

1. Open **Edit > Plugins**.
2. Confirm **Niagara** is enabled.
3. Confirm **Weather Environment System** is enabled.
4. Restart Unreal Editor if either plugin was just enabled.
5. In the Content Browser settings, enable **Show Plugin Content** and **Show Engine Content**.

Create project-owned lightning assets under a game-content folder such as:

```text
/Game/Weather/VFX/Lightning
```

Keeping the authored effect in game content prevents a future plugin update from overwriting it. The weather profile can reference a Niagara system from any mounted content folder.

## 2. Understand the current presenter contract

When a deterministic storm-cell timer expires, the lightning presenter:

1. selects a deterministic XY location inside the cell;
2. traces to terrain or water when tracing is enabled;
3. resolves the final strike location;
4. creates a Niagara component at:

```text
StrikeLocation + (0, 0, Presentation.Lightning.SpawnHeight)
```

5. writes these Niagara parameters;
6. activates the system once;
7. destroys the component when Niagara reports `OnSystemFinished`.

The component location is the beam source. `User.WeatherLightningTarget` is the ground/water endpoint. No separate source parameter is required.

## 3. Create the exact user-parameter contract

The C++ presenter uses `SetVariableVec3` for vector values, so choose Niagara **Vector**/**Vector3**, not Niagara **Position**, for the exposed values in this version of the plugin.

The complete lightning system needs these parameters:

| Name shown in Niagara | Type | Preview default |
| --- | --- | ---: |
| `User.WeatherCellCenter` | Vector3 | `(0, 0, 0)` |
| `User.WeatherLightningTarget` | Vector3 | `(0, 0, -150000)` |
| `User.WeatherLightningIntensity` | Float | `1.0` |

The target preview value produces a 1.5-kilometre downward bolt when the preview component is at the origin, matching the profile's default `150000` cm spawn height.

Parameter names and types must match exactly. The runtime will not update a Position-typed target or a differently capitalized name when the profile mapping still points to `User.WeatherLightningTarget` as a Vector3.

`User.WeatherCellCenter` is part of the shared presentation contract. A basic primary bolt does not need to read it, but secondary branch or cloud-flash emitters may use it later.

## 4. Create the Niagara system from Static Beam

1. Open `/Game/Weather/VFX/Lightning` in the Content Browser.
2. Right-click empty space and choose **FX > Niagara System**.
3. Choose **New system from selected emitters**.
4. Find **Static Beam** under the emitter templates.
5. Press **+** to add it to the selected-emitter list.
6. Click **Finish**.
7. Name the system `NS_WeatherLightning`.
8. Open it and rename the emitter `NE_WeatherLightning_Bolt`.
9. Create the three User Parameters from the previous section and give them the preview defaults.
10. Save the system.

The Static Beam template already supplies the essential beam modules and a Ribbon Renderer. If the template is unavailable, create a Minimal emitter and reproduce the stack listed in the next section.

## 5. Confirm the primary-bolt stack

The emitter should contain this structure:

```text
Emitter Update
    Emitter State
    Beam Emitter Setup
    Spawn Burst Instantaneous

Particle Spawn
    Initialize Particle
    Spawn Beam
    Beam Width

Particle Update
    Particle State
    Color
    Update Beam
    Jitter Position

Render
    Ribbon Renderer
```

Important ordering rules:

- `Spawn Beam` must be present in Particle Spawn.
- `Update Beam` must be above `Jitter Position` in Particle Update.
- Use one burst to create the ordered beam points.
- Do not add a continuous Spawn Rate module to the primary bolt.

## 6. Make the entire effect finite

This is the most important integration step. `UWeatherLightningPresenter` retains an active-effect slot until the Niagara system finishes.

### System State

1. Select **System State**.
2. Set **Loop Behavior** to **Once**.
3. Set a short fixed **Loop Duration**, initially `0.30` seconds.
4. Disable loop delay.
5. Set **Inactive Response** to **Complete**.

### Emitter State

1. Select `NE_WeatherLightning_Bolt > Emitter State`.
2. Set **Life Cycle Mode** to **Self**.
3. Set **Loop Behavior** to **Once**.
4. Set **Loop Duration Mode** to **Fixed** if that option is displayed.
5. Set **Loop Duration** to approximately `0.20–0.25` seconds.
6. Set **Loop Count** to `1` if the field remains visible.
7. Disable loop delay.
8. Set **Inactive Response** to **Complete**.

Every optional emitter later added to this Niagara system must also be finite. One infinite glow, spark, light, or cloud-flash emitter prevents `OnSystemFinished`, permanently occupies one of the presenter's active-effect slots, and eventually makes later strikes lose their Niagara visual.

## 7. Configure CPU simulation and bounds

A lightning bolt uses relatively few particles and at most a small configured number of concurrent effects. CPU simulation is a reliable starting point for beam/ribbon ordering.

1. Select the emitter properties.
2. Set **Sim Target** to **CPU Sim**.
3. Disable **Local Space**.
4. Leave **Calculate Bounds Mode** on **Dynamic** for the initial CPU version.

If the final effect is changed to GPU simulation, use fixed bounds that cover the complete downward bolt and its jitter relative to the component source. For the default spawn height, start near:

```text
Minimum: (-25000, -25000, -175000)
Maximum: ( 25000,  25000,   25000)
```

Increase XY bounds if the bolt uses larger branches or displacement. Increase negative Z magnitude whenever the profile's **Spawn Height** exceeds `150000` cm.

## 8. Bind the beam start to the component origin

Select **Beam Emitter Setup**.

1. Enable **Absolute Start**.
2. Set **Beam Start** to `Engine.Owner.Position` if the Static Beam template is not already using the system/component position.
3. Keep the emitter in world space.

At runtime, the weather presenter places the Niagara component above the resolved strike. `Engine.Owner.Position` is therefore the exact source location.

For the initial result, disable **Use Beam Tangents**. Jitter alone will produce an irregular bolt. Add tangents only after the straight endpoint contract is proven.

## 9. Bind Beam End to the weather target

`Beam End` expects an absolute beam endpoint, while the plugin currently supplies that absolute world location as a Vector3 user parameter.

1. In **Beam Emitter Setup**, enable **Absolute End**.
2. Open the value menu beside **Beam End**.
3. Select **New Dynamic Scratch Input**.
4. In the dynamic-input graph, remove the unused default input if necessary.
5. Add `User.WeatherLightningTarget` to the Map Get node.
6. Connect it to the output expected by **Beam End**.
7. Niagara should insert a **Vector3 to Position** conversion automatically. If it does not, drag from the Vector3 pin and search for a Position conversion before connecting the output.
8. Click **Apply** and return to System Overview.
9. Confirm the Beam End field now displays the Scratch dynamic input and that `User.WeatherLightningTarget` has one read.

The preview should now draw from `(0, 0, 0)` down to `(0, 0, -150000)`.

Do not create an Actor Component Interface endpoint for this system. That workflow is useful for a manually placed beam, but the weather presenter already sends the resolved target directly every strike.

## 10. Configure burst density and lifetime

### Spawn Burst Instantaneous

The default bolt spans up to `150000` cm, so it needs more sample points than a short weapon beam.

1. Set **Spawn Time** to `0`.
2. Start with **Spawn Count** between `48` and `96`.
3. Use `64` as the initial value.

More points create finer displacement but increase ribbon and CPU work. Tune this while inspecting the full runtime bolt length, not only a shortened preview.

### Initialize Particle

1. Set **Lifetime** to approximately `0.15–0.25` seconds.
2. Start with a cool white-blue color.
3. Keep alpha at `1.0` while establishing the beam.

The system duration must remain long enough for all particles to expire. If particle lifetime is `0.25`, use a system duration comfortably above it, such as `0.30–0.35` seconds.

## 11. Shape the bolt width

Select **Beam Width** under Particle Spawn.

1. Use **Float from Curve** or the equivalent curve input.
2. Choose a ramp-up/ramp-down curve so both endpoints taper.
3. Start with a maximum primary width of approximately `50–150` cm for the 1.5-kilometre open-world bolt.

Add **Beam Width Scale** under Particle Update when available:

```text
BeamWidthScale = User.WeatherLightningIntensity
```

The presenter supplies intensity in the normalized `0–1` range. If a zero-width strike is undesirable, remap it:

```text
BeamWidthScale = Lerp(0.35, 1.0, User.WeatherLightningIntensity)
```

Keep width art direction separate from burst count. Adding more beam particles increases geometric detail, not physical thickness.

## 12. Add large-scale lightning jitter

Select **Jitter Position** in Particle Update and verify **Update Beam** is immediately above it.

For an open-world bolt that is approximately 1.5 kilometres long, begin with:

| Setting | Initial value |
| --- | ---: |
| Jitter Amount | `1000–4000` cm |
| Jitter Delay | a small negative value such as `-0.01` |
| Curve Tension | approximately `0.5` in Ribbon Renderer |

A negative jitter delay updates the displacement rapidly and creates the characteristic crackling shape during the short lifetime. If the result vibrates too violently, reduce jitter amount first and then increase delay toward zero.

The values in Epic's short-beam example are much smaller because that example is not spanning a kilometre-scale weather strike.

## 13. Configure color from lightning intensity

Select **Color** under Particle Update.

1. Choose a base color such as pale blue-white.
2. Build a dynamic input that multiplies the base Linear Color by `User.WeatherLightningIntensity`.
3. If low-intensity strikes become invisible, remap intensity before multiplication:

```text
VisualIntensity = Lerp(0.4, 1.0, User.WeatherLightningIntensity)
```

The final production material should use Particle Color for emissive brightness and opacity. Keep extreme emissive values in the material rather than encoding HDR values into the normalized weather intensity contract.

## 14. Configure the Ribbon Renderer

1. Select **Ribbon Renderer**.
2. Use Niagara's `DefaultRibbonMaterial` while validating endpoints.
3. Set **Facing Mode** to **Screen** initially.
4. Confirm **Position Binding** is `Particles.Position`.
5. Confirm **Color Binding** is `Particles.Color`.
6. Confirm **Ribbon Link Order Binding** uses `Particles.RibbonLinkOrder` when the Static Beam template exposes it.
7. Set **Curve Tension** near `0.5`.
8. Keep tessellation moderate until the final runtime length is visible.

For the production material:

- use an unlit additive or translucent material;
- multiply emissive and opacity by Particle Color;
- create a bright narrow core and softer outer falloff across the ribbon width;
- avoid expensive refraction;
- test bloom at the project's actual exposure settings;
- ensure the material is two-sided or appropriate for the chosen ribbon shape/facing mode.

Compile and save the Niagara system. In the preview, it should flash once, disappear, and then restart only when the preview is reset—not loop continuously.

## 15. Assign the lightning system to the weather profile

1. Open the `WeatherEnvironmentProfile` assigned to the level's `WeatherEnvironmentController`.
2. Expand **Presentation > Lightning**.
3. Enable **Enabled**.
4. Assign `NS_WeatherLightning` to **Lightning System**.
5. Start with these production-oriented values:

| Profile setting | Initial value |
| --- | ---: |
| Maximum Active Effects | `4` |
| Presentation Radius In Cells | `6.0` |
| Minimum Lightning Potential | `0.5` |
| Minimum Strike Interval Seconds | `8.0` |
| Maximum Strike Interval Seconds | `30.0` |
| Low Potential Interval Multiplier | `2.0` |
| Random Seed Salt | `5179` |
| Cell Edge Inset Fraction | `0.1` |
| Center Weight | `0.35` |
| Trace To Ground | Enabled |
| Require Ground Trace Hit | Disabled initially |
| Ground Trace Channel | Visibility |
| Ground Trace Height | `250000` cm |
| Ground Trace Depth | `500000` cm |
| Spawn Height | `150000` cm |
| Speed Of Sound | `34300` cm/s |
| Maximum Thunder Delay Seconds | `20.0` |

6. Expand **Presentation > Niagara Parameters**.
7. Confirm the mappings are exact:

```text
Cell Center:          User.WeatherCellCenter
Lightning Target:     User.WeatherLightningTarget
Lightning Intensity:  User.WeatherLightningIntensity
```

8. Confirm that the controller's **Environment Profile** points to this edited profile.
9. Save the profile and level.

## 16. Force a controlled storm test

Use a duplicate test profile so production weather distribution and strike timing remain intact.

1. Duplicate the active profile and name it something like `DA_Weather_LightningTest`.
2. Assign the duplicate to the level controller.
3. Under **Simulation > Front Lifecycle**, disable the lifecycle temporarily.
4. Clear **Initial Seeds** and set **Initial Generated Seed Count** to `0`.
5. Under **Simulation > Baseline Values**, set:

```text
Storminess:          1.0
Lightning Potential: 1.0
Rain Intensity:      0.8
```

6. Set **Minimum Weather Type Duration Seconds** to `0` for the test.
7. Confirm the storm enter threshold remains at or below `0.65`.
8. Under **Presentation > Lightning**, temporarily set strike intervals to `1–2` seconds.
9. Keep **Require Ground Trace Hit** disabled for the first test, ensuring a trace miss still uses the safe grid-centre fallback.
10. Start PIE from a fresh session.
11. Inspect the runtime controller's `LightningPresenter`:

```text
Scheduled Cell Count > 0
Active Effect Count  returns to 0 after every short strike
```

12. Enable **Require Ground Trace Hit** only after confirming the landscape/water responds to the selected trace channel.
13. Restore the production profile and normal strike intervals after testing.

The most important completion check is that **Active Effect Count** rises during a strike and returns to zero. If it only rises, something inside the Niagara system is still looping or never completing.

## 17. Connect the synchronized Blueprint hooks

The Niagara system renders the bolt. The controller delegates synchronize lighting, camera effects, gameplay, and audio.

### Immediate flash and gameplay event

Bind to the level controller's **On Lightning Strike** delegate. It supplies:

```text
Location
Intensity
Cell Coord
```

Use this event to:

- move a reusable Point Light to `Location`;
- enable it for approximately `0.05–0.15` seconds;
- scale light intensity from the normalized strike `Intensity`;
- trigger a brief global/directional-light exposure flash when desired;
- start camera shake based on player distance;
- notify gameplay systems or AI.

Use a retained timer handle to disable the flash light. Avoid spawning a permanent light actor for every strike.

### Delayed thunder

Bind to **On Thunder Due** on the same controller. It fires after:

```text
Distance from nearest local view / Speed Of Sound
```

Use **Play Sound at Location** with the delegate's strike location. Do not add the main thunder directly to the Niagara system, because Niagara activates at strike time while this delegate supplies the physically delayed playback time.

An optional close crack can play immediately from **On Lightning Strike**, with the lower-frequency thunder reserved for **On Thunder Due**.

## 18. Add optional visual layers safely

After the primary bolt completes and releases correctly, optional finite emitters can add:

- a short-lived cloud-source flash;
- ground sparks at `User.WeatherLightningTarget`;
- a small impact glow;
- secondary ribbon branches;
- a rapidly expanding light sprite.

For every added emitter:

1. use **Loop Behavior: Once**;
2. use a finite burst;
3. keep particle lifetime shorter than the overall system completion window;
4. confirm Active Effect Count still returns to zero;
5. ensure its bounds include the source or target region it renders.

Do not add the delayed thunder as a long-lifetime Niagara emitter. Use the controller's `OnThunderDue` event instead so the visual component can be released immediately after the flash.

## 19. Production checks

Before considering the effect finished, verify all of the following:

- The system and every emitter use **Loop Behavior: Once**.
- The primary emitter uses one **Spawn Burst Instantaneous** and no continuous Spawn Rate.
- Beam Start follows `Engine.Owner.Position`.
- Beam End reads `User.WeatherLightningTarget`.
- `User.WeatherLightningTarget` is Vector3, matching the current C++ setter.
- The preview bolt travels downward by the expected spawn height.
- Runtime ground traces place the endpoint on terrain/water or use the configured fallback.
- `Update Beam` appears above `Jitter Position`.
- Ribbon bounds cover the complete bolt and displacement.
- `User.WeatherLightningIntensity` changes width/brightness without changing the deterministic location.
- Active Effect Count returns to zero after every strike.
- Pool exhaustion drops only the Niagara visual; `On Lightning Strike` and `On Thunder Due` still fire.
- Thunder uses the delayed controller event rather than Niagara lifetime.
- Production intervals are restored after the short forced-storm test.

## Troubleshooting

### The beam appears in Niagara preview but not in PIE

- Confirm `NS_WeatherLightning` is assigned under **Presentation > Lightning > Lightning System**.
- Confirm the local cell has authoritative `bIsStorm` true and meets **Minimum Lightning Potential**.
- Confirm all user-parameter names and types match exactly.
- Temporarily reduce strike intervals to `1–2` seconds.
- Inspect **Scheduled Cell Count** to ensure the camera is within the presentation radius.

### The beam starts correctly but ends at the world origin

- Confirm Beam End uses the dynamic Scratch input.
- Confirm it reads `User.WeatherLightningTarget`, not the preview constant.
- Confirm the Vector3-to-Position conversion is present.
- Confirm the profile parameter mapping was not renamed.

### The beam points upward or begins at the ground

- Beam Start must use `Engine.Owner.Position`.
- Beam End must use `User.WeatherLightningTarget`.
- The presenter places the component above the target by **Spawn Height**; do not add another upward spawn-height offset in Niagara.

### Strikes stop showing after several successful flashes

- Inspect **Active Effect Count**.
- Find any emitter still set to Infinite or Multiple loops.
- Check for particles with infinite lifetime.
- Check optional audio/light emitters that outlive the bolt.
- Verify the system actually reaches `OnSystemFinished`.

### Lightning never reaches terrain

- Confirm **Trace To Ground** is enabled.
- Confirm the landscape/water blocks the configured **Ground Trace Channel**.
- Increase trace height/depth if the world vertical range exceeds the defaults.
- Leave **Require Ground Trace Hit** disabled until collision responses are correct.

### The bolt is smooth instead of jagged

- Confirm `Update Beam` is above `Jitter Position`.
- Increase Jitter Amount for the kilometre-scale bolt.
- Use a slightly negative Jitter Delay.
- Increase burst count if segments are too coarse.

### The effect is too expensive

- Reduce burst count before reducing the presenter's active-effect cap.
- Reduce ribbon tessellation and branch count.
- Keep optional sparks and flashes short-lived.
- Profile with four simultaneous effects, matching the default maximum.
- Avoid costly translucent refraction across a kilometre-scale ribbon.

## Official Niagara references

- [Beam effect for Unreal Engine 5.7](https://dev.epicgames.com/documentation/unreal-engine/how-to-create-a-beam-effect-in-niagara-for-unreal-engine?application_version=5.7)
- [Ribbon effect workflow](https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-create-a-ribbon-effect-in-niagara-for-unreal-engine)
- [Niagara Ribbon Renderer reference](https://dev.epicgames.com/documentation/unreal-engine/render-module-reference-for-niagara-effects-in-unreal-engine)
- [Niagara tutorials for Unreal Engine 5.7](https://dev.epicgames.com/documentation/unreal-engine/tutorials-for-niagara-effects-in-unreal-engine?application_version=5.7)

