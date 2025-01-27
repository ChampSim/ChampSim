#!/usr/bin/env python3

"""Generate a compile_commands.json file for the ChampSim source code."""

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


def create_main_compile_command_(
    build_id: str,
    champsim_dir: Path = DEFAULT_CHAMPSIM_DIR,
    config_dir: Path = DEFAULT_CONFIG_DIR,
) -> CompileCommand:
    """Create the compile command for main.cc.

    :param build_id: The ChampSim build ID.
    :param champsim_dir: Path to the ChampSim repository.
    :param config_dir: Path to the ChampSim config directory.
    :return: Compile command for main.cc.
    """
    file: Final[Path] = champsim_dir / "src" / "main.cc"
    object_file: Final[Path] = config_dir / f"{build_id}_main.o"

    return CompileCommand(
        arguments=[
            os.environ.get("CXX", "g++"),
            *get_options(champsim_dir / "global.options"),
            *get_options(champsim_dir / "absolute.options"),
            f"-I{config_dir}",
            f"-DCHAMPSIM_BUILD=0x{build_id}",
            "-c",
            "-o",
            f"{object_file.absolute()}",
            f"{file.absolute()}",
        ],
        directory=champsim_dir,
        file=file,
        output=object_file,
    )


def create_src_compile_command_(
    file: Path,
    champsim_dir: Path = DEFAULT_CHAMPSIM_DIR,
    config_dir: Path = DEFAULT_CONFIG_DIR,
) -> CompileCommand:
    """Create the compile command for a source file.

    :param file: Path to the source file.
    :param champsim_dir: Path to the ChampSim repository.
    :param config_dir: Path to the ChampSim config directory.
    :return: Compile command for the source file.
    """
    object_file: Final[Path] = config_dir / file.relative_to(
        champsim_dir / "src"
    ).with_suffix(".o")
    return CompileCommand(
        arguments=[
            os.environ.get("CXX", "g++"),
            *get_options(champsim_dir / "global.options"),
            *get_options(champsim_dir / "absolute.options"),
            f"-I{config_dir}",
            "-c",
            "-o",
            f"{object_file.absolute()}",
            f"{file.absolute()}",
        ],
        directory=champsim_dir,
        file=file,
        output=object_file,
    )


def create_src_compile_command(
    file: Path,
    build_id: str = "0",
    champsim_dir: Path = DEFAULT_CHAMPSIM_DIR,
    config_dir: Path = DEFAULT_CONFIG_DIR,
) -> CompileCommand:
    """Create the compile command for a source file.

    :param file: Path to the file.
    :param build_id: The ChampSim build ID.
    :param champsim_dir: Path to the ChampSim repository.
    :param config_dir: Path to the ChampSim config directory.
    :return Compile command for the source file.
    """
    if file.parts[-1] == "main.cc":
        return create_main_compile_command_(
            build_id, champsim_dir=champsim_dir, config_dir=config_dir
        )
    else:
        return create_src_compile_command_(
            file, champsim_dir=champsim_dir, config_dir=config_dir
        )


def main():
    """Generate a compile_commands.json file for the ChampSim source code."""
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--build-id",
        type=str,
        help="The build ID",
        required=True,
    )
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
        args.champsim_dir / "src",
        extensions=EXTENSIONS,
        create_fn=create_src_compile_command,
        champsim_dir=args.champsim_dir,
        config_dir=args.config_dir,
        build_id=args.build_id,
    )

    manifest.save(indent=args.indent)


if __name__ == "__main__":
    main()
