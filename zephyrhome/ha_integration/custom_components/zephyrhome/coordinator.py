"""ZephyrHome DataUpdateCoordinator — polls device via CoAP."""
from __future__ import annotations

import json
import logging
from datetime import timedelta

from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

_LOGGER = logging.getLogger(__name__)
SCAN_INTERVAL = timedelta(seconds=30)

# Resource paths probed to discover component names
_KNOWN_SENSOR_NAMES: list[str] = []   # populated from /zephyrhome/info
_KNOWN_SWITCH_NAMES: list[str] = []


class ZHCoordinator(DataUpdateCoordinator):
    """Fetches data from a ZephyrHome device over CoAP."""

    def __init__(self, hass: HomeAssistant, device_addr: str, entry_id: str) -> None:
        super().__init__(
            hass,
            _LOGGER,
            name=f"ZephyrHome {device_addr}",
            update_interval=SCAN_INTERVAL,
        )
        self.device_addr = device_addr
        self.entry_id = entry_id
        self._coap_ctx = None
        self.device_info: dict = {}
        self.sensor_names: list[str] = []
        self.switch_names: list[str] = []

    async def _async_setup(self) -> None:
        try:
            import aiocoap
            self._coap_ctx = await aiocoap.Context.create_client_context()
            # Fetch device info once at setup to discover component names
            info = await self._get_json("zephyrhome/info")
            self.device_info = info
        except ImportError:
            raise UpdateFailed("aiocoap is not installed")
        except Exception as exc:
            raise UpdateFailed(f"Failed to reach device [{self.device_addr}]: {exc}") from exc

    async def _async_update_data(self) -> dict:
        """Fetch all sensor and switch values."""
        if self._coap_ctx is None:
            await self._async_setup()

        data: dict = {"sensors": {}, "switches": {}}

        # Re-fetch info occasionally to pick up new components
        try:
            info = await self._get_json("zephyrhome/info")
            self.device_info = info
        except Exception:
            pass

        for name in self.sensor_names:
            try:
                val = await self._get_json(f"zephyrhome/sensors/{name}")
                data["sensors"][name] = val
            except Exception as exc:
                _LOGGER.warning("Sensor '%s' read error: %s", name, exc)

        for name in self.switch_names:
            try:
                val = await self._get_json(f"zephyrhome/switches/{name}")
                data["switches"][name] = val
            except Exception as exc:
                _LOGGER.warning("Switch '%s' read error: %s", name, exc)

        return data

    async def _get_json(self, path: str) -> dict:
        import aiocoap

        uri = f"coap://[{self.device_addr}]/{path}"
        req = aiocoap.Message(code=aiocoap.GET, uri=uri)
        resp = await self._coap_ctx.request(req).response

        if not resp.code.is_successful():
            raise UpdateFailed(f"CoAP GET {path} failed: {resp.code}")

        return json.loads(resp.payload.decode())

    async def async_set_switch(self, name: str, state: bool) -> None:
        """Send PUT /zephyrhome/switches/{name} with new state."""
        import aiocoap

        uri = f"coap://[{self.device_addr}]/zephyrhome/switches/{name}"
        payload = json.dumps({"state": state}).encode()
        req = aiocoap.Message(code=aiocoap.PUT, uri=uri, payload=payload)
        resp = await self._coap_ctx.request(req).response

        if not resp.code.is_successful():
            raise UpdateFailed(f"PUT switch '{name}' failed: {resp.code}")

        # Optimistic update
        if self.data:
            self.data["switches"].setdefault(name, {})["state"] = state
            self.async_update_listeners()
