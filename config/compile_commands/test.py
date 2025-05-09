#!/usr/bin/env python3

"""Generate a compile_commands.json file for the ChampSim test code."""

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

EXTENSIONS: Final[List[str]] = ["cc"]


def create_test_compile_command(
    file: Path,
    champsim_dir: Path = DEFAULT_CHAMPSIM_DIR,
    config_dir: Path = DEFAULT_CONFIG_DIR,
) -> CompileCommand:
    """Create the compile command for a test source file.

    :param file: Path to the source file.
    :param champsim_dir: Path to the ChampSim repository.
    :param config_dir: Path to the ChampSim config directory.
    :return: Compile command for the test source file.
    """
    object_file: Final[Path] = (
        config_dir
        / "test"
        / file.relative_to(champsim_dir / "test" / "cpp" / "src").with_suffix(".o")
    )
    return CompileCommand(
        arguments=[
            os.environ.get("CXX", "g++"),
            *get_options(champsim_dir / "global.options"),
            *get_options(champsim_dir / "absolute.options"),
            f"-I{config_dir}",
            "-I.",
            "-DCHAMPSIM_TEST_BUILD",
            "-g3",
            "-Og",
            "-c",
            "-o",
            f"{object_file.absolute()}",
            f"{file.absolute()}",
        ],
        directory=champsim_dir,
        file=file,
        output=object_file,
    )


def main():
    """Generate a compile_commands.json file for a ChampSim module."""
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
        help="The number of spaces to indent the JSON file",
        default=DEFAULT_INDENT,
    )

    args = parser.parse_args()

    manifest = CompileCommandManifest.Create(
        args.champsim_dir / "test" / "cpp" / "src",
        extensions=EXTENSIONS,
        create_fn=create_test_compile_command,
        champsim_dir=args.champsim_dir,
        config_dir=args.config_dir,
    )

    manifest.save(indent=args.indent)


if __name__ == "__main__":
    main()
