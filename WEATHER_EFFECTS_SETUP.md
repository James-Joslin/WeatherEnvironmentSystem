# Weather Effects Setup

Stage 5 presents the Stage 4 CPU weather field through a bounded Niagara rain pool and deterministic one-shot lightning. Both adapters are optional: missing Niagara assets disable only their visuals, while clock, simulation, queries, strike hooks, and thunder timing remain operational.

For complete editor walkthroughs, see:

- [NIAGARA_RAIN_SYSTEM_SETUP.md](NIAGARA_RAIN_SYSTEM_SETUP.md) for the camera-local rain graph and controlled rain test;
- [NIAGARA_LIGHTNING_SYSTEM_SETUP.md](NIAGARA_LIGHTNING_SYSTEM_SETUP.md) for the one-shot Static Beam, endpoint binding, completion lifecycle, flash hooks, and controlled storm test.

## Profile setup

Open the `WeatherEnvironmentProfile` assigned to the level's `WeatherEnvironmentController`, then expand **Presentation**.

Under **Precipitation**:

- Assign a Niagara rain system to **Rain System** and leave **Enabled** on.
- **Pool Size** is the hard maximum number of rain components. Components are created lazily and retained for reuse.
- **Presentation Radius In Cells** defaults to three current grid-cell widths around each local player view.
- **Cell Overlap Fraction** extends each rain volume past its cell edge to hide seams.
- **Maximum Spawn Rate** is multiplied by the cell's normalized rain intensity.
- **Fade In Seconds** and **Fade Out Seconds** control emission transitions before a component is recycled.
- **Intensity Priority Weight** and **Proximity Priority Weight** decide which cells win when more nearby cells are raining than the pool can cover.

The authoritative `bIsRaining` cell value controls activation. Stage 4 hysteresis therefore prevents threshold noise from repeatedly assigning and releasing rain components. Intensity and wind can continue changing on an assigned component without a respawn.

Under **Lightning**:

- Assign a non-looping Niagara strike system to **Lightning System**.
- **Maximum Active Effects** bounds concurrent strike visuals. Strike and thunder delegates still fire when this visual pool is full.
- **Presentation Radius In Cells** limits deterministic per-cell timers to locally relevant storm cells.
- A cell must have authoritative `bIsStorm` set and meet **Minimum Lightning Potential**.
- **Minimum/Maximum Strike Interval Seconds**, **Low Potential Interval Multiplier**, the Stage 4 environment seed, and **Random Seed Salt** produce deterministic per-cell schedules.
- **Cell Edge Inset Fraction** and **Center Weight** control the deterministic XY strike distribution.
- **Trace To Ground** traces the configured channel through terrain or water. By default a failed trace safely uses the grid centre Z; enable **Require Ground Trace Hit** to discard that strike instead.
- **Spawn Height** places the Niagara component above the resolved target.
- Thunder delay is nearest-view distance divided by **Speed Of Sound**, capped by **Maximum Thunder Delay Seconds**.

## Niagara user parameters

The Niagara systems must expose parameters matching **Presentation > Niagara Parameters**. Names are profile-editable; defaults are:

| Parameter | Type | Consumer |
| --- | --- | --- |
| `User.WeatherCellCenter` | Vector3 | Rain and lightning |
| `User.WeatherCellExtent` | Vector3 | Rain |
| `User.WeatherRainIntensity` | Float | Rain |
| `User.WeatherWindVector` | Vector3 | Rain |
| `User.WeatherSpawnRate` | Float | Rain |
| `User.WeatherTransitionAlpha` | Float | Rain |
| `User.WeatherLightningTarget` | Vector3 | Lightning |
| `User.WeatherLightningIntensity` | Float | Lightning |

Multiply the rain emitter's spawn rate by `WeatherTransitionAlpha`, or use it to fade renderer opacity as well. The component deactivates and becomes reusable only after fade-out reaches zero.

The lightning system must complete rather than loop. Its `OnSystemFinished` notification releases the transient component; a looping system will occupy its active-effect slot indefinitely.

## Authoring an effective rain system

Rain and lightning require opposite Niagara lifecycle settings. The rain system must remain active continuously while its pooled component is assigned to a weather cell. The presenter fades `WeatherTransitionAlpha` to zero and then explicitly deactivates the component. It does not expect the rain system to complete by itself.

For the rain system and each of its emitters, use:

- **Life Cycle Mode:** Self;
- **Loop Behavior:** Infinite;
- continuous spawn-rate modules instead of repeating bursts;
- **Local Space:** disabled, because cell centres, extents, and wind are supplied in world space;
- automatic completion/deactivation disabled;
- GPU simulation where the target platform supports it.

If the rain system completes on its own, the weather cell can remain assigned to an inactive pooled component until that component is eventually recycled. Lightning must instead use a finite, non-looping system so `OnSystemFinished` can release its active-effect slot.

### Recommended emitter layers

One rain system can contain two GPU sprite emitters:

- **Near rain:** thicker and longer streaks at a moderate particle count;
- **Far rain:** thinner and shorter streaks at a higher particle count.

Both emitters should align sprites to velocity and use the same weather parameters. Start with approximately 25–40 percent of the supplied spawn rate for near rain and 60–75 percent for far rain.

Suggested initial values, expressed in Unreal centimetres, are:

| Property | Near rain | Far rain |
| --- | ---: | ---: |
| Streak width | 1–3 cm | 0.5–1.5 cm |
| Streak length | 75–200 cm | 30–100 cm |
| Downward speed | 5,000–9,000 cm/s | 3,000–7,000 cm/s |
| Lifetime | 0.5–1.5 seconds | 1–2 seconds |

Add `User.WeatherWindVector` to the downward velocity. Keep most vertical speed independent of the wind so strong horizontal weather tilts the rain without making it appear to float.

### Weather-cell scale and camera-local spawning

The default weather cell is `100000` cm, or one kilometre, wide. The presenter currently supplies an XY half-extent of approximately `55000` cm with the default ten-percent overlap. Uniformly spawning rain throughout that complete volume would spend most particles outside the visible area.

Use `User.WeatherCellCenter` and `User.WeatherCellExtent` as clipping boundaries, while generating particles inside a camera-local volume. Good starting dimensions are:

- near-rain radius: 3,000–5,000 cm;
- far-rain radius: 6,000–10,000 cm;
- spawn height: 5,000–12,000 cm above the camera;
- kill plane: several thousand centimetres below the camera.

Read the current camera or view position in Niagara, generate a position inside that smaller volume, and reject or clamp positions outside:

```text
WeatherCellCenter - WeatherCellExtent
    to
WeatherCellCenter + WeatherCellExtent
```

This makes adjacent raining cells meet cleanly at their boundaries while limiting actual particle work to the visible region. **Cell Overlap Fraction** provides a small additional margin that hides numerical or renderer seams.

### Spawn rate and appearance

The presenter already calculates:

```text
User.WeatherSpawnRate = MaximumSpawnRate * RainIntensity
```

Each Niagara emitter should calculate its effective rate as:

```text
EffectiveSpawnRate =
    User.WeatherSpawnRate
    * User.WeatherTransitionAlpha
    * EmitterLayerFraction
```

Do not multiply by rain intensity again unless a deliberately nonlinear density response is required. Use `User.WeatherRainIntensity` separately to control qualities such as opacity, streak length, mist amount, splash probability, turbulence, and material brightness.

The profile default permits up to 20,000 particles per second per assigned cell. Treat this as an upper bound. Begin around 4,000–8,000 per cell, inspect the result with several adjacent cells active, and raise it only while GPU time and translucent overdraw remain acceptable.

### Bounds, culling, and collision

The pooled Niagara component remains anchored at its weather-cell centre even when particles are generated around the camera. GPU emitters therefore need fixed bounds covering one complete cell half-extent, the overlap margin, and particle wind travel. With the default grid, start near `(-65000, -65000, -120000)` to `(65000, 65000, 120000)` in component-local space. Avoid bounds spanning several cells, but do not shrink them to only the camera-local spawn radius or rain can be culled near a cell edge.

For efficient presentation:

- use an unlit rain material and keep translucent screen coverage under review;
- align sprite streaks to particle velocity;
- keep far-rain collision disabled;
- restrict scene-depth or distance-field collision to the near emitter when collision is necessary;
- prefer a separate, low-rate splash emitter instead of generating a splash for every colliding drop;
- configure Niagara Effect Type scalability so it does not cull rain inside the profile's presentation radius;
- verify that inactive pooled components remain deactivated and that only nearby selected cells have active Niagara simulations.

## Blueprint hooks

Bind directly on the level's `WeatherEnvironmentController`:

- **On Lightning Strike** fires immediately with world location, normalized intensity, and cell coordinate. Use it for a directional/point-light flash, camera shake, gameplay reactions, or immediate crack audio.
- **On Thunder Due** fires with the same payload after the distance-based acoustic delay. Use it to play the main thunder audio at or relative to the supplied strike location.

The `LightningPresenter` component exposes the same delegates plus `Get Last Thunder Delay Seconds`, `Get Scheduled Cell Count`, and `Get Active Effect Count`. The `PrecipitationPresenter` exposes enable state, allocated pool count, active cell count, and presented-cell queries.

## Validation

Run the Stage 5 automation group from **Tools > Test Automation**:

```text
WeatherEnvironment.Stage5
```

The tests cover range filtering, pool exhaustion priority, authoritative rain hysteresis, fade progression, missing assets, deterministic lightning timing and placement, and both trace-failure policies. In PIE, also fly across adjacent rainy cells to inspect overlap and confirm that authored Niagara systems respond to every configured user parameter.
