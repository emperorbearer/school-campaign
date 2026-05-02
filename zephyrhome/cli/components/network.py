"""OpenThread network configuration code generator."""
from __future__ import annotations

from .base import GeneratorContext
from ..schema import NetworkConfig


def contribute_network(cfg: NetworkConfig, ctx: GeneratorContext) -> None:
    """Append OpenThread Kconfig lines based on network config."""
    pan_id_int = int(cfg.pan_id, 16)
    ctx.add_kconfig(
        # Networking stack
        "CONFIG_NETWORKING=y",
        "CONFIG_NET_UDP=y",
        "CONFIG_NET_IPV6=y",
        "CONFIG_NET_L2_OPENTHREAD=y",
        # OpenThread MTD Sleepy End Device (ultra low power)
        "CONFIG_OPENTHREAD_MTD=y",
        "CONFIG_OPENTHREAD_MTD_SED=y",
        f"CONFIG_OPENTHREAD_POLL_PERIOD=5000",
        # CoAP
        "CONFIG_COAP=y",
        # Power management
        "CONFIG_PM=y",
        "CONFIG_PM_DEVICE=y",
        "CONFIG_PM_DEVICE_RUNTIME=y",
        # Thread network credentials (consumed by zh_ot_init.c)
        f"CONFIG_ZH_OT_CHANNEL={cfg.channel}",
        f"CONFIG_ZH_OT_PANID={cfg.pan_id}",
        f'CONFIG_ZH_OT_NETWORK_NAME="ZephyrHome"',
        f'CONFIG_ZH_OT_NETWORK_KEY="{cfg.network_key}"',
        # OTA via MCUBoot
        "CONFIG_BOOTLOADER_MCUBOOT=y",
        "CONFIG_IMG_MANAGER=y",
        # Logging
        "CONFIG_LOG=y",
        "CONFIG_LOG_DEFAULT_LEVEL=3",
        # OpenThread stack size
        "CONFIG_OPENTHREAD_THREAD_STACK_SIZE=3072",
    )

    coap_server = cfg.coap_server or "ff03::1"
    ctx.add_kconfig(f'CONFIG_ZH_COAP_SERVER="{coap_server}"')
