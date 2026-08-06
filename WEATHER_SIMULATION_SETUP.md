# Weather Simulation Setup

Stage 4 turns the CPU weather grid into a deterministic field of moving Gaussian fronts. Wind remains owned by Stage 3; seeds sample that field and carry cloud, humidity, temperature, pressure, rain, storm, and lightning values through it.

## Profile setup

Open the `WeatherEnvironmentProfile` assigned to the level's `WeatherEnvironmentController`, then expand **Simulation**.

- Leave **Enabled** on for normal fixed-step simulation.
- **Fixed Update Interval Seconds** defaults to `0.033333333` (30 Hz).
- **Baseline Weight** keeps every cell valid when no seed reaches it. The baseline values describe uncovered weather.
- **Boundary Policy** defaults to **Wrap**. **Clamp** holds seeds at the grid edge; **Expire** removes them when they leave. **Expire** is recommended when replacements should cross the world once; **Wrap** works with the lifecycle's finite lifetimes when fronts should be allowed to circulate.
- **Environment Seed** controls all profile-sampled values and procedural seed generation.
- **Maximum Seed Count** is a hard allocation/work guard.
- **Initial Seeds** provides explicitly authored world-XY seed positions.
- **Initial Generated Seed Count** retains the original deterministic, evenly cycled generator for manual/testing workflows. New profiles leave it at zero because **Front Lifecycle** owns production population density.
- **Use Cell Relative Generated Sigma** defaults on. The `0.35–0.65` range is expressed in cell widths, keeping generated fronts useful when grid cell size changes. Disable it only when an advanced setup requires an absolute centimetre range.
- **Weather Type Presets** materialize a seed's values when **Use Weather Type Preset** is enabled. The profile starts with useful Clear through Storm presets.

`Sigma` is the Gaussian standard deviation in centimetres. A seed is evaluated only within three sigma. Larger values make broader fronts; `Strength` changes the seed's normalized influence when fronts overlap. `Movement Multiplier` scales the sampled local wind velocity. A non-positive lifetime is infinite.

## Production front lifecycle

Expand **Simulation > Front Lifecycle** to configure long-running large-world variation. It is enabled by default and calculates:

```text
Target fronts = ceil(Grid Cell Count / Target Cells Per Front)
```

The result is clamped by **Minimum Front Count**, **Maximum Front Count**, and the simulation's global **Maximum Seed Count**. Defaults use one front per 16 cells, with a minimum of 4 and maximum of 32.

The initial managed population is spread through the grid using deterministic max-min placement. Later replacements are placed along the upwind boundary, derived from the average current grid wind with **Wind > Default Direction** as the zero-wind fallback. **Minimum Spacing Cell Widths** discourages immediate overlap; when all attempts fail, the best available candidate is used so the population cannot remain permanently starved.

**Archetypes** control each automatically generated weather kind independently:

- Clear: weight 20, broad and gentle;
- Partly Cloudy: weight 30, broad and long-lived;
- Overcast: weight 25;
- Rain: weight 15;
- Heavy Rain: weight 7, smaller and stronger;
- Storm: weight 3, compact, strong, fast, and short-lived.

Weights are relative and deterministic, not percentages that must total 100. Each archetype supplies its own cell-relative Sigma, strength, real-second lifetime, and movement-multiplier ranges. It resolves continuous weather values from the matching entry in **Weather Type Presets**; an archetype without a matching preset is skipped.

Every replenishment interval, at most **Maximum Spawns Per Interval** replacements are added. Initial population filling is immediate. Existing managed fronts are never deleted merely because a target setting is reduced; the population settles naturally as fronts expire.

### Existing profiles

Version-six profile migration enables the lifecycle and replaces only the untouched legacy default of four generated seeds. Explicitly authored **Initial Seeds** and custom **Initial Generated Seed Count** values are preserved.

External seeds count toward the target by default. For a clean lifecycle-owned world, clear **Initial Seeds** and set **Initial Generated Seed Count** to zero. To retain authored/gameplay fronts while maintaining the complete managed target in addition to them, disable **Count External Seeds Toward Target**. Restart PIE after changing initial seeds because they are materialized once per Game Instance session.

## Classification asset

Create **Miscellaneous > Data Asset > WeatherTypeLookupDataAsset** and assign it to **Simulation > Weather Type Lookup**.

Rules are evaluated in array order. The highest matching **Priority** wins, and the earlier rule wins an equal-priority tie. Each continuous range is inclusive and can be disabled independently. Rain and storm booleans can be ignored, required true, or required false. A current rule's ranges expand by its **Hysteresis** value, while **Minimum Weather Type Duration Seconds** prevents an early transition to a different type.

If no lookup asset is assigned, the runtime uses the built-in Clear, Partly Cloudy, Overcast, Rain, Heavy Rain, and Storm classifier. Clear remains the guaranteed fallback when an asset has no matching rule.

## Blueprint API

Get the `WeatherStateSubsystem` from the Game Instance. It exposes:

- `Add Seed`, `Remove Seed`, `Move Seed`, and `Clear Seeds`;
- `Generate Seed Set` and `Get Active Seeds`;
- `Get Target Weather Front Count` and `Replenish Weather Fronts`;
- `Step Simulation` for an exact fixed-step advance;
- `Get Weather At Location` and `Get Weather Type At Location`;
- `On Weather Type Changed` and debounced `On Local Weather Changed` events.

An invalid ID passed to `Add Seed` is replaced with a deterministic stable ID. A supplied duplicate ID or a seed beyond the configured cap is rejected and returns an invalid ID.

Continuous point queries are bilinear across neighboring cell centres and interpolate between completed fixed steps. The returned weather type and rain/storm booleans come from the containing authoritative cell, so gameplay categories never become half-transitioned presentation values.

## Runtime behavior

Every fixed step performs these operations in order:

1. Sample the bilinear local Stage 3 wind at each seed.
2. Advect positions and apply lifetime/boundary rules.
3. Replenish a depleted managed population when its real-time interval elapses.
4. Accumulate normalized Gaussian values only in each seed's three-sigma cell neighborhood.
5. Clamp normalized fields and derive hysteretic rain/storm booleans.
6. Classify cells and publish type/local-weather events.

Generated positions are stratified across the grid before deterministic jitter is applied, avoiding an initial cluster in one corner. The same profile, grid, fixed steps, and seed commands produce the same seed IDs, sampled values, positions, cell fields, and classifications.

## Viewport debugging

After compiling the plugin and restarting Unreal Editor, select the level's `WeatherEnvironmentController` and expand **Weather Grid Debug**:

1. Enable **Enabled** and leave **Draw Only When Selected** enabled for the normal workflow.
2. Enable **Draw Coordinates** and **Draw Weather Type** under **Labels**. Enable the numerical fields only when needed to keep the viewport readable.
3. Use Simulate or PIE to inspect the live Stage 4 subsystem grid. The non-playing editor viewport displays the controller's preview grid.

The labels are rendered directly to the editor viewport canvas and do not require a GameMode HUD class. Weather colors are cyan-blue for Clear, pale cyan for Partly Cloudy, grey for Overcast, blue for Rain or Heavy Rain, and purple for Storm. `Draw Distance` can limit labels and geometry around the current editor view; zero displays every cell.
