"""Audit and compile the migrated project foliage through the plugin validator."""

import unreal


COMMAND = "Weather.ValidateSpatialFoliageMaterials"

unreal.log(f"WEATHER_FOLIAGE_VALIDATION_BEGIN {COMMAND}")
unreal.SystemLibrary.execute_console_command(None, COMMAND)
unreal.log(f"WEATHER_FOLIAGE_VALIDATION_END {COMMAND}")
