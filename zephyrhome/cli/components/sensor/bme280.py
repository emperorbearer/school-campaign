"""BME280 temperature/humidity sensor code generator."""
from __future__ import annotations

from ..base import ComponentGenerator, GeneratorContext
from ...schema import BME280SensorConfig


class BME280Generator(ComponentGenerator):
    config: BME280SensorConfig

    def contribute(self, ctx: GeneratorContext) -> None:
        cfg = self.config
        safe = self.c_safe(cfg.name)
        addr_hex = f"{cfg.address:02x}"

        # Kconfig
        ctx.add_kconfig(
            "CONFIG_I2C=y",
            "CONFIG_SENSOR=y",
            "CONFIG_BME280=y",
        )

        # Device tree overlay: add BME280 node on the I2C bus
        ctx.overlay_nodes.append(
            f"&{cfg.i2c_bus} {{\n"
            f"\t{safe}: bme280@{addr_hex} {{\n"
            f"\t\tcompatible = \"bosch,bme280\";\n"
            f"\t\treg = <0x{addr_hex}>;\n"
            f"\t\tstatus = \"okay\";\n"
            f"\t}};\n"
            f"}};"
        )

        # C include and init call
        include = '#include "zh_sensor_generic.h"'
        if include not in ctx.c_includes:
            ctx.c_includes.append(include)

        ctx.c_init_calls.append(
            f'zh_sensor_register("{cfg.name}", '
            f"DEVICE_DT_GET(DT_NODELABEL({safe})), "
            f"{cfg.update_interval_seconds}U);"
        )

        ctx.add_cmake_component("sensor_generic")
