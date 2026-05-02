"""ZephyrHome Home Assistant integration.

Discovers ZephyrHome devices on the local Thread/IPv6 network via CoAP
multicast announcements and exposes their sensors and switches as HA entities.

Setup flow:
  1. HA calls async_setup() when the integration is loaded.
  2. async_setup_entry() is called for each config entry (device).
  3. The coordinator polls the device via CoAP and forwards data to entities.
"""
from __future__ import annotations

import asyncio
import json
import logging
from dataclasses import dataclass, field

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant

from .const import DOMAIN, COAP_PORT, ANNOUNCE_MCAST

_LOGGER = logging.getLogger(__name__)

PLATFORMS = ["sensor", "switch"]


@dataclass
class ZHDeviceInfo:
    """Info received from a device announcement or /zephyrhome/info GET."""
    address: str          # IPv6 unicast of the device
    name: str             # e.g. "living-room-sensor"
    friendly_name: str    # e.g. "거실 센서"
    fw_version: str
    component_count: int
    sensors: list[str] = field(default_factory=list)
    switches: list[str] = field(default_factory=list)


async def async_setup(hass: HomeAssistant, config: dict) -> bool:
    hass.data.setdefault(DOMAIN, {})
    return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    from .coordinator import ZHCoordinator

    device_addr = entry.data["device_addr"]
    coordinator = ZHCoordinator(hass, device_addr, entry.entry_id)

    await coordinator.async_config_entry_first_refresh()
    hass.data[DOMAIN][entry.entry_id] = coordinator

    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    unloaded = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unloaded:
        hass.data[DOMAIN].pop(entry.entry_id, None)
    return unloaded


# ── Discovery helper (used by config_flow) ──────────────────────────────────

async def discover_devices(timeout: float = 10.0) -> list[ZHDeviceInfo]:
    """
    Listen for ZephyrHome multicast announcements on the local network.

    Devices POST to coap://[ff03::1]/zephyrhome/announce; we receive them
    by acting as a CoAP server bound to the multicast group.

    Returns a list of discovered ZHDeviceInfo within `timeout` seconds.
    """
    try:
        import aiocoap
        import aiocoap.resource as resource
    except ImportError:
        _LOGGER.error("aiocoap is required for ZephyrHome discovery")
        return []

    found: list[ZHDeviceInfo] = []

    class AnnounceResource(resource.Resource):
        async def render_post(self, request):
            try:
                data = json.loads(request.payload.decode())
                src = str(request.remote.hostinfo)
                info = ZHDeviceInfo(
                    address=src,
                    name=data.get("name", src),
                    friendly_name=data.get("fn", data.get("name", src)),
                    fw_version=data.get("fw", "unknown"),
                    component_count=data.get("components", 0),
                )
                if not any(d.name == info.name for d in found):
                    found.append(info)
                    _LOGGER.info("Discovered ZephyrHome device: %s at [%s]",
                                 info.name, src)
            except Exception as exc:
                _LOGGER.debug("Announce parse error: %s", exc)
            return aiocoap.Message(code=aiocoap.CHANGED)

    site = resource.Site()
    site.add_resource(["zephyrhome", "announce"], AnnounceResource())

    ctx = await aiocoap.Context.create_server_context(
        site, bind=("::", COAP_PORT)
    )

    await asyncio.sleep(timeout)
    await ctx.shutdown()

    return found
