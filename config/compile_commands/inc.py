#!/usr/bin/env python3

"""Generate a compile_commands.json file for the ChampSim headers."""

import argparse
import os
from pathlib import Path
from typing import Final, List

from common import (
    DEFAULT_CHAMPSIM_DIR,
    DEFAULT_CONFIG_DIR,
    DEFAULT_INDENT,
    CompileCommand,
    CompileCommandManifest,
    get_options,
)

EXTENSIONS: Final[List[str]] = ["h", "hpp"]


def create_inc_compile_command(
    file: Path,
    champsim_dir: Path = DEFAULT_CHAMPSIM_DIR,
    config_dir: Path = DEFAULT_CONFIG_DIR,
) -> CompileCommand:
    """Create the compile command for an include file.

    :param inc_file: Path to the include file.
    :param champsim_dir: Path to the ChampSim repository.
    :param config_dir: Path to the ChampSim config directory.
    :return: Compile command for the include file.
    """
    return CompileCommand(
        arguments=[
            os.environ.get("CXX", "g++"),
            *get_options(champsim_dir / "global.options"),
            *get_options(champsim_dir / "absolute.options"),
            f"-I{config_dir}",
        ],
        directory=champsim_dir,
        file=file,
        output=None,
    )


def main():
    """Generate a compile_commands.json file for include files."""
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--champsim-dir",
        type=Path,
        help="The path to the ChampSim repository",
        default=DEFAULT_CHAMPSIM_DIR,
    )
    parser.add_argument(
        "--config-dir",
        type=Path,
        help="The path to the ChampSim config directory",
        default=DEFAULT_CONFIG_DIR,
    )
    parser.add_argument(
        "--indent",
        type=int,
        help="Number of spaces to indent the generated JSON file by",
        default=DEFAULT_INDENT,
    )

    args = parser.parse_args()

    manifest = CompileCommandManifest.Create(
        args.champsim_dir / "inc",
        extensions=EXTENSIONS,
        create_fn=create_inc_compile_command,
        champsim_dir=args.champsim_dir,
        config_dir=args.config_dir,
    )

    manifest.save(indent=args.indent)


if __name__ == "__main__":
    main()
