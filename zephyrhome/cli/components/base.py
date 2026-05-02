"""Abstract base class for all ZephyrHome component code generators."""
from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class GeneratorContext:
    """Accumulated code fragments that component generators contribute to."""
    output_dir: Path
    device_name: str
    # Kconfig lines appended to prj.conf
    kconfig_lines: list[str] = field(default_factory=list)
    # Device tree overlay snippets
    overlay_nodes: list[str] = field(default_factory=list)
    # #include lines for generated_components.c
    c_includes: list[str] = field(default_factory=list)
    # Function calls inside zh_components_init()
    c_init_calls: list[str] = field(default_factory=list)
    # Component subdirectory names under firmware/components/
    cmake_component_dirs: list[set] = field(default_factory=list)

    def add_kconfig(self, *lines: str) -> None:
        for line in lines:
            if line not in self.kconfig_lines:
                self.kconfig_lines.append(line)

    def add_cmake_component(self, dirname: str) -> None:
        if dirname not in self.cmake_component_dirs:
            self.cmake_component_dirs.append(dirname)


class ComponentGenerator(ABC):
    """
    Base for all platform-specific code generators.
    Subclasses implement contribute() to deposit snippets into GeneratorContext.
    """

    def __init__(self, config) -> None:
        self.config = config

    @abstractmethod
    def contribute(self, ctx: GeneratorContext) -> None:
        """Deposit Kconfig lines, DTS overlay nodes, and C init calls."""
        ...

    @staticmethod
    def c_safe(name: str) -> str:
        """Convert component name to a valid C/DTS identifier."""
        return name.replace("-", "_")
