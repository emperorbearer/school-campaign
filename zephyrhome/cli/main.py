"""ZephyrHome CLI entry point."""
from __future__ import annotations

from pathlib import Path

import click
import yaml

from .schema import ZephyrHomeConfig
from .generator import generate_project
from .uploader import west_build, west_flash, coap_fota_upload


def _load_config(config_file: Path) -> ZephyrHomeConfig:
    raw = yaml.safe_load(config_file.read_text())
    return ZephyrHomeConfig.model_validate(raw)


def _default_build_dir(config_file: Path, cfg: ZephyrHomeConfig) -> Path:
    return config_file.parent / ".zephyrhome_build" / cfg.device.name


@click.group()
@click.version_option("0.1.0", prog_name="zephyrhome")
def cli() -> None:
    """ZephyrHome — ESPHome-style YAML-to-firmware for Zephyr/Thread devices."""


@cli.command()
@click.argument("config_file", type=click.Path(exists=True, path_type=Path))
@click.option(
    "--output-dir", "-o",
    type=click.Path(path_type=Path),
    default=None,
    help="Where to write the generated Zephyr project (default: .zephyrhome_build/<name>/).",
)
@click.option("--no-build", is_flag=True, help="Generate files only; skip west build.")
def compile(config_file: Path, output_dir: Path | None, no_build: bool) -> None:
    """Validate YAML, generate a Zephyr project, and run west build."""
    cfg = _load_config(config_file)
    out = output_dir or _default_build_dir(config_file, cfg)

    click.echo(f"[zephyrhome] Generating project for '{cfg.device.name}' → {out}")
    generate_project(cfg, out)
    click.secho("  Generated files: prj.conf, app.overlay, CMakeLists.txt, src/", fg="green")

    if no_build:
        return

    click.echo(f"[zephyrhome] Running west build -b {cfg.device.board}")
    click.echo("  (Make sure ZEPHYR_BASE and ZEPHYRHOME_BASE are set in your environment)")
    west_build(out, cfg.device.board)
    click.secho("  Build complete.", fg="green")


@cli.command()
@click.argument("config_file", type=click.Path(exists=True, path_type=Path))
@click.option(
    "--method",
    type=click.Choice(["west", "coap"]),
    default="west",
    show_default=True,
    help="'west' uses west flash; 'coap' uploads via CoAP FOTA.",
)
@click.option("--device-addr", default=None, help="Device IPv6 address for CoAP FOTA.")
@click.option("--output-dir", "-o", type=click.Path(path_type=Path), default=None)
def upload(
    config_file: Path,
    method: str,
    device_addr: str | None,
    output_dir: Path | None,
) -> None:
    """Flash firmware to the device (west flash or CoAP FOTA)."""
    cfg = _load_config(config_file)
    out = output_dir or _default_build_dir(config_file, cfg)

    if method == "west":
        click.echo(f"[zephyrhome] Flashing via west flash (build dir: {out})")
        west_flash(out)
        click.secho("  Flash complete.", fg="green")
    else:
        if not device_addr:
            raise click.UsageError("--device-addr is required for --method=coap")
        fw = out / "build" / "zephyr" / "app_update.bin"
        if not fw.exists():
            raise click.ClickException(f"Firmware not found: {fw}\nRun 'zephyrhome compile' first.")
        click.echo(f"[zephyrhome] CoAP FOTA upload to [{device_addr}] …")
        coap_fota_upload(device_addr, fw)
        click.secho("  FOTA complete.", fg="green")


@cli.command()
@click.argument("config_file", type=click.Path(exists=True, path_type=Path))
@click.option("--output-dir", "-o", type=click.Path(path_type=Path), default=None)
@click.pass_context
def run(ctx: click.Context, config_file: Path, output_dir: Path | None) -> None:
    """Compile then flash via west (shortcut for compile + upload)."""
    ctx.invoke(compile, config_file=config_file, output_dir=output_dir, no_build=False)
    ctx.invoke(upload, config_file=config_file, method="west",
               device_addr=None, output_dir=output_dir)


@cli.command()
@click.argument("config_file", type=click.Path(exists=True, path_type=Path))
def validate(config_file: Path) -> None:
    """Check YAML configuration for errors without generating any files."""
    try:
        cfg = _load_config(config_file)
        click.secho(f"Config valid: {cfg.device.name} ({cfg.device.board})", fg="green")
        click.echo(f"  Sensors : {len(cfg.sensors)}")
        click.echo(f"  Switches: {len(cfg.switches)}")
    except Exception as exc:
        raise click.ClickException(str(exc)) from exc
