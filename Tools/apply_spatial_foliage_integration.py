"""Run the compiled, idempotent legacy-foliage spatial wind integration."""

import unreal


COMMAND = "Weather.RetargetLegacyFoliageMaterials"

unreal.log(f"WEATHER_FOLIAGE_MIGRATION_BEGIN {COMMAND}")
unreal.SystemLibrary.execute_console_command(None, COMMAND)
unreal.log(f"WEATHER_FOLIAGE_MIGRATION_END {COMMAND}")
