# Weather sky-dome setup and migration

`M_WeatherSkyDome` is the default sky path. It is an Opaque, Unlit, Two Sided Surface material with **Is Sky** enabled, not a post-process material. `AWeatherEnvironmentController` creates its runtime MID, assigns it to a large sphere, scales that sphere from the profile, and follows the active player camera.

This ordering is the key correction: the dome is the distant opaque background, Sky Atmosphere is composed with it, and volumetric clouds render in front. The old post-process method replaced final scene pixels after cloud composition, so no depth mask could make it a reliable cloud background.

## Migrate an existing level

1. Restart Unreal Editor after rebuilding the plugin.
2. Enable **Show Plugin Content** in the Content Browser.
3. Confirm `/WeatherEnvironmentSystem/Materials/M_WeatherSkyDome` exists. If it does not, run `Weather.GenerateSkyDomeMaterial` in the editor console.
4. Open every unbound Post Process Volume and remove the old `PostProcess_Stars_Mat2`, `M_WeatherSkyboxPostProcess`, and instances derived from either material. Keep the volume and all of its non-sky settings.
5. Open the `WeatherEnvironmentProfile` used by the level.
6. Enable **Skybox > Enabled** and disable **Use Legacy Post Process Sky**.
7. Leave **Sky Dome Material** empty to use the supplied master automatically, or assign an instance derived from `M_WeatherSkyDome`.
8. Keep **Center Sky Dome On Player Camera** enabled. Start with **Sky Dome Radius** `10,000,000` cm.
9. Place exactly one `WeatherEnvironmentController` and assign the profile, sun, moon, and Skylight references.
10. Enter PIE. The controller's **Sky Dome Mesh** is hidden until the runtime material is initialised.

An instance derived from the old post-process material cannot be reused as the new dome material because its Material Domain and render path are different. Create a new instance from `M_WeatherSkyDome` instead.

## Lighting and cloud components

Use this level arrangement:

- a Movable sun Directional Light with **Atmosphere Sun Light Index** `0`;
- a Movable moon Directional Light with **Atmosphere Sun Light Index** `1`;
- one Sky Atmosphere;
- one Volumetric Cloud;
- one Skylight, preferably using **Real Time Capture** for a continuously changing sky;
- one Weather Environment Controller.

The moon still needs enough intensity to light the cloud volume. If that makes the landscape too bright, lower the moon's direct intensity and raise the Volumetric Cloud component's **Cloud Scattering Luminance Scale** to style the cloud response independently. Exposure settings in the Post Process Volume also remain important at night.

## Profile controls

- **Layers 0–3:** cubemap, hue, saturation, luminosity, mip level, and optional luminosity-by-day-fraction curve.
- **Gradient / Tint:** world-direction gradient shaping and final colour treatment.
- **Maximum Brightness:** clamps the completed custom sky.
- **Rotation Degrees:** rotates all cubemap directions around world Z.
- **Day Night:** solar-elevation range and direction gate used to hide night cubemaps during the day.
- **Sky Atmosphere Luminance Multiplier:** amount of Sky Atmosphere view luminance combined with the custom layers. Atmosphere Fade Strength and its optional sun-elevation curve further scale this value.
- **Sky Dome Radius:** sphere radius in centimetres.
- **Center Sky Dome On Player Camera:** prevents the player from approaching the background and avoids open-world parallax.

The controller updates the runtime MID from the profile. Editing an assigned Material Instance is useful for parent defaults, but values exposed in the profile are written again during play.

## Validation

Test in PIE or Standalone, not only in the static editor viewport:

- view the centre and all four corners at horizontal FOV `60`, `90`, `120`, and `150`;
- move rapidly through the world and confirm the sky never approaches or develops translation parallax;
- advance through sunrise and sunset and confirm the night cubemaps fade at the configured solar elevations;
- look through thick and thin volumetric clouds and confirm stars remain behind them;
- lower moon intensity and tune cloud scattering independently;
- confirm no weather or legacy stars material remains in any Post Process Volume.

## Legacy fallback

`M_WeatherSkyboxPostProcess` and `Weather.GenerateSkyboxMaterial` remain for compatibility. To use them, enable **Use Legacy Post Process Sky** in the profile. That route uses an unbound controller-owned Post Process Component and should not be combined with the dome. It can solve wide-FOV depth-mask leakage, but because it runs as a post effect it cannot provide the same natural volumetric-cloud ordering as the opaque sky dome.
