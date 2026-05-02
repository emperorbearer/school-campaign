"""Generic Zephyr sensor API component generator (for sensors with DT aliases)."""
from __future__ import annotations

from ..base import ComponentGenerator, GeneratorContext


class GenericSensorGenerator(ComponentGenerator):
    def contribute(self, ctx: GeneratorContext) -> None:
        cfg = self.config
        ctx.add_kconfig("CONFIG_SENSOR=y")
        include = '#include "zh_sensor_generic.h"'
        if include not in ctx.c_includes:
            ctx.c_includes.append(include)
        ctx.c_init_calls.append(
            f'zh_sensor_register("{cfg.name}", '
            f"DEVICE_DT_GET(DT_NODELABEL({self.c_safe(cfg.name)})), "
            f"{cfg.update_interval_seconds}U);"
        )
        ctx.add_cmake_component("sensor_generic")
