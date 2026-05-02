"""ZephyrHome sensor entities for Home Assistant."""
from __future__ import annotations

from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorStateClass,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import UnitOfTemperature
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
        ZHTemperatureSensor(coordinator, name)
        for name in coordinator.sensor_names
    ]
    async_add_entities(entities)


class ZHTemperatureSensor(CoordinatorEntity, SensorEntity):
    """Temperature value from a ZephyrHome sensor component."""

    _attr_device_class = SensorDeviceClass.TEMPERATURE
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = UnitOfTemperature.CELSIUS

    def __init__(self, coordinator: ZHCoordinator, sensor_name: str) -> None:
        super().__init__(coordinator)
        self._sensor_name = sensor_name
        self._attr_unique_id = f"{coordinator.device_addr}_{sensor_name}_temp"
        device_fn = coordinator.device_info.get("fn", coordinator.device_addr)
        self._attr_name = f"{device_fn} {sensor_name} Temperature"

    @property
    def native_value(self) -> float | None:
        if not self.coordinator.data:
            return None
        reading = self.coordinator.data["sensors"].get(self._sensor_name)
        return reading.get("v") if reading else None

    @property
    def extra_state_attributes(self) -> dict:
        if not self.coordinator.data:
            return {}
        reading = self.coordinator.data["sensors"].get(self._sensor_name, {})
        return {k: v for k, v in reading.items() if k != "v"}
