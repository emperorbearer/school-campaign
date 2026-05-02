"""GPIO switch component code generator."""
from __future__ import annotations

import re
from ..base import ComponentGenerator, GeneratorContext
from ...schema import GPIOSwitchConfig


def _parse_pin(pin: str) -> tuple[int, int]:
    """'P0.13' → (port=0, pin=13)"""
    m = re.fullmatch(r"P(\d+)\.(\d+)", pin)
    if not m:
        raise ValueError(f"Invalid pin format: {pin}")
    return int(m.group(1)), int(m.group(2))


class GPIOSwitchGenerator(ComponentGenerator):
    config: GPIOSwitchConfig

    def contribute(self, ctx: GeneratorContext) -> None:
        cfg = self.config
        safe = self.c_safe(cfg.name)
        port, pin = _parse_pin(cfg.pin)
        active_flag = "GPIO_ACTIVE_HIGH" if cfg.active_high else "GPIO_ACTIVE_LOW"

        ctx.add_kconfig("CONFIG_GPIO=y")

        # Device tree: add a gpio-leds node for the switch
        ctx.overlay_nodes.append(
            f"/ {{\n"
            f"\tzh_switches {{\n"
            f"\t\tcompatible = \"gpio-leds\";\n"
            f"\t\t{safe}_gpio: {safe} {{\n"
            f"\t\t\tgpios = <&gpio{port} {pin} {active_flag}>;\n"
            f"\t\t\tlabel = \"{cfg.friendly_name or cfg.name}\";\n"
            f"\t\t}};\n"
            f"\t}};\n"
            f"}};"
        )

        include = '#include "zh_switch_gpio.h"'
        if include not in ctx.c_includes:
            ctx.c_includes.append(include)

        ctx.c_init_calls.append(
            f'zh_switch_gpio_register("{cfg.name}", '
            f"DT_GPIO_CTLR(DT_NODELABEL({safe}_gpio), gpios), "
            f"DT_GPIO_PIN(DT_NODELABEL({safe}_gpio), gpios), "
            f"DT_GPIO_FLAGS(DT_NODELABEL({safe}_gpio), gpios));"
        )

        ctx.add_cmake_component("switch_gpio")
