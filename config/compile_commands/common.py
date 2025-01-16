"""Common default values and functions for generating compile_commands.json files."""

from pathlib import Path
from typing import Final, List

DEFAULT_CHAMPSIM_DIR: Final[Path] = Path(".")
DEFAULT_CONFIG_DIR: Final[Path] = Path(".csconfig")


def get_options(options_file: Path) -> List[str]:
    """Read the compile options from a .options file.

    :param options_file: Path to the .options file.
    :return: List of compile options.
    """
    with options_file.open() as f:
        return f.read().split()