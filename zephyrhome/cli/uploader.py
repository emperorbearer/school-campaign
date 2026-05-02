"""Firmware upload: west flash (local) and CoAP FOTA (remote)."""
from __future__ import annotations

import asyncio
import subprocess
from pathlib import Path


def west_build(project_dir: Path, board: str) -> None:
    """Run `west build` for the generated project."""
    build_dir = project_dir / "build"
    cmd = [
        "west", "build",
        "-b", board,
        "--build-dir", str(build_dir),
        str(project_dir),
    ]
    subprocess.run(cmd, check=True)


def west_flash(project_dir: Path) -> None:
    """Run `west flash` using the build directory."""
    build_dir = project_dir / "build"
    subprocess.run(["west", "flash", "--build-dir", str(build_dir)], check=True)


def coap_fota_upload(device_addr: str, firmware_path: Path) -> None:
    """
    Upload firmware via CoAP BLOCK1 transfer to /zephyrhome/ota on device.
    Requires aiocoap installed.
    """
    asyncio.run(_fota_async(device_addr, firmware_path))


async def _fota_async(device_addr: str, firmware_path: Path) -> None:
    try:
        import aiocoap
        import aiocoap.numbers.codes as codes
    except ImportError:
        raise RuntimeError("aiocoap is required for CoAP FOTA. Install with: pip install aiocoap")

    data = firmware_path.read_bytes()
    ctx = await aiocoap.Context.create_client_context()

    uri = f"coap://[{device_addr}]/zephyrhome/ota"
    request = aiocoap.Message(
        code=aiocoap.PUT,
        uri=uri,
        payload=data,
    )
    # aiocoap handles BLOCK1 fragmentation automatically
    response = await ctx.request(request).response
    await ctx.shutdown()

    if response.code.is_successful():
        print(f"FOTA upload complete: {response.code}")
    else:
        raise RuntimeError(f"FOTA upload failed: {response.code} — {response.payload!r}")
