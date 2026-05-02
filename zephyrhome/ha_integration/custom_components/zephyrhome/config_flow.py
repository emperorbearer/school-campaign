"""Config flow for ZephyrHome integration."""
from __future__ import annotations

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.core import HomeAssistant

from .const import DOMAIN, CONF_DEVICE_ADDR, CONF_DEVICE_NAME

STEP_USER_SCHEMA = vol.Schema({
    vol.Required(CONF_DEVICE_ADDR): str,
    vol.Optional(CONF_DEVICE_NAME, default=""): str,
})


class ZephyrHomeConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a ZephyrHome config flow (manual IP entry for MVP)."""

    VERSION = 1

    async def async_step_user(self, user_input=None):
        errors = {}

        if user_input is not None:
            addr = user_input[CONF_DEVICE_ADDR].strip()

            # Quick reachability check
            try:
                from . import ZHDeviceInfo
                from .coordinator import ZHCoordinator
                import aiocoap, json

                ctx = await aiocoap.Context.create_client_context()
                req = aiocoap.Message(
                    code=aiocoap.GET,
                    uri=f"coap://[{addr}]/zephyrhome/info",
                )
                resp = await ctx.request(req).response
                await ctx.shutdown()

                info = json.loads(resp.payload.decode())
                name = info.get("name", addr)

                await self.async_set_unique_id(f"zephyrhome_{name}")
                self._abort_if_unique_id_configured()

                return self.async_create_entry(
                    title=info.get("fn", name),
                    data={
                        CONF_DEVICE_ADDR: addr,
                        CONF_DEVICE_NAME: name,
                        "sensors": [],
                        "switches": [],
                    },
                )
            except Exception:
                errors["base"] = "cannot_connect"

        return self.async_show_form(
            step_id="user",
            data_schema=STEP_USER_SCHEMA,
            errors=errors,
        )
