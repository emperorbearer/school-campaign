"""Pydantic models for ZephyrHome YAML configuration validation."""
from __future__ import annotations

import re
from typing import Literal, Optional, Union
from pydantic import BaseModel, Field, field_validator, model_validator


class DeviceConfig(BaseModel):
    name: str = Field(pattern=r"^[a-z0-9][a-z0-9\-]*$")
    board: str
    friendly_name: Optional[str] = None

    @field_validator("name")
    @classmethod
    def name_no_underscores(cls, v: str) -> str:
        if "_" in v:
            raise ValueError("device name must use hyphens, not underscores")
        return v


class NetworkConfig(BaseModel):
    type: Literal["openthread"] = "openthread"
    channel: int = Field(default=15, ge=11, le=26)
    pan_id: str = Field(default="0xABCD")
    network_key: str = Field(
        default="00112233445566778899aabbccddeeff",
        description="32 hex chars (16 bytes)",
    )
    coap_server: Optional[str] = None  # IPv6 unicast; None → multicast ff03::1

    @field_validator("pan_id")
    @classmethod
    def valid_pan_id(cls, v: str) -> str:
        v = v.strip()
        try:
            val = int(v, 16)
        except ValueError:
            raise ValueError("pan_id must be hex (e.g. 0xABCD)")
        if not (0x0000 <= val <= 0xFFFE):
            raise ValueError("pan_id must be 0x0000–0xFFFE")
        return v

    @field_validator("network_key")
    @classmethod
    def valid_network_key(cls, v: str) -> str:
        key = v.replace(" ", "").replace(":", "")
        if not re.fullmatch(r"[0-9a-fA-F]{32}", key):
            raise ValueError("network_key must be 32 hex characters (16 bytes)")
        return key.lower()


def _parse_interval(v: str) -> int:
    """Convert '60s', '5m', '1h' to seconds."""
    v = v.strip()
    if v.endswith("h"):
        return int(v[:-1]) * 3600
    if v.endswith("m"):
        return int(v[:-1]) * 60
    if v.endswith("s"):
        return int(v[:-1])
    return int(v)


class BME280SensorConfig(BaseModel):
    platform: Literal["bme280"]
    name: str = Field(pattern=r"^[a-z0-9][a-z0-9_]*$")
    i2c_bus: str = "i2c0"
    address: Union[int, str] = 0x76
    update_interval: str = "60s"
    friendly_name: Optional[str] = None

    @field_validator("address", mode="before")
    @classmethod
    def parse_address(cls, v: Union[int, str]) -> int:
        if isinstance(v, str):
            return int(v, 16)
        return v

    @property
    def update_interval_seconds(self) -> int:
        return _parse_interval(self.update_interval)


class DHT22SensorConfig(BaseModel):
    platform: Literal["dht22"]
    name: str = Field(pattern=r"^[a-z0-9][a-z0-9_]*$")
    pin: str
    update_interval: str = "30s"
    friendly_name: Optional[str] = None

    @property
    def update_interval_seconds(self) -> int:
        return _parse_interval(self.update_interval)


SensorConfig = Union[BME280SensorConfig, DHT22SensorConfig]


class GPIOSwitchConfig(BaseModel):
    platform: Literal["gpio"]
    name: str = Field(pattern=r"^[a-z0-9][a-z0-9_]*$")
    pin: str  # e.g. "P0.13"
    active_high: bool = True
    friendly_name: Optional[str] = None

    @field_validator("pin")
    @classmethod
    def valid_pin(cls, v: str) -> str:
        if not re.fullmatch(r"P\d+\.\d+", v):
            raise ValueError("pin must be in format P<port>.<pin>, e.g. P0.13")
        return v


SwitchConfig = Union[GPIOSwitchConfig]


class ZephyrHomeConfig(BaseModel):
    device: DeviceConfig
    network: NetworkConfig = Field(default_factory=NetworkConfig)
    sensors: list[SensorConfig] = Field(default_factory=list)
    switches: list[SwitchConfig] = Field(default_factory=list)

    @model_validator(mode="after")
    def check_unique_names(self) -> "ZephyrHomeConfig":
        all_names = [s.name for s in self.sensors] + [s.name for s in self.switches]
        seen = set()
        for name in all_names:
            if name in seen:
                raise ValueError(f"duplicate component name: '{name}'")
            seen.add(name)
        return self
