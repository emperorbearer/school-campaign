"""ZephyrHome switch entities for Home Assistant."""
from __future__ import annotations

from homeassistant.components.switch import SwitchEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import ZHCoordinator


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    coordinator: ZHCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities = [
        ZHSwitch(coordinator, name)
        for name in coordinator.switch_names
    ]
    async_add_entities(entities)


class ZHSwitch(CoordinatorEntity, SwitchEntity):
    """A switch entity backed by a ZephyrHome GPIO switch component."""

    def __init__(self, coordinator: ZHCoordinator, switch_name: str) -> None:
        super().__init__(coordinator)
        self._switch_name = switch_name
        self._attr_unique_id = f"{coordinator.device_addr}_{switch_name}_switch"
        device_fn = coordinator.device_info.get("fn", coordinator.device_addr)
        self._attr_name = f"{device_fn} {switch_name}"

    @property
    def is_on(self) -> bool | None:
        if not self.coordinator.data:
            return None
        reading = self.coordinator.data["switches"].get(self._switch_name)
        return reading.get("state") if reading else None

    async def async_turn_on(self, **kwargs) -> None:
        await self.coordinator.async_set_switch(self._switch_name, True)

    async def async_turn_off(self, **kwargs) -> None:
        await self.coordinator.async_set_switch(self._switch_name, False)
