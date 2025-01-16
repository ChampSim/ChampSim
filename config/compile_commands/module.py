#!/usr/bin/env python3

"""Generate a compile_commands.json file for a ChampSim module."""

import argparse
import json
import os
from pathlib import Path
from typing import Any, Dict, Final, List

from common import DEFAULT_CHAMPSIM_DIR, DEFAULT_CONFIG_DIR, get_options


def create_module_compile_commands(
    module_dir: Path,
    champsim_dir: Path = DEFAULT_CHAMPSIM_DIR,
    config_dir: Path = DEFAULT_CONFIG_DIR,
) -> None:
    """Create the compile_commands.json file for a ChampSim module's files.

    :param module_dir: The path to the module's directory
    :param champsim_dir: The path to the ChampSim repository
    :param config_dir: The path to the ChampSim config directory
    """
    src_files: Final[List[Path]] = list(module_dir.glob("**/*.cc"))
    entries: List[Dict[str, Any]] = []

    for src_file in src_files:
        obj_file: Path = (
            config_dir
            / "test"
            / src_file.relative_to(module_dir).with_suffix(".o")
        )
        entries.append({
            "arguments": [
                os.environ.get("CXX", "g++"),
                *get_options(champsim_dir / "global.options"),
                *get_options(champsim_dir / "absolute.options"),
                *get_options(champsim_dir / "module.options"),
                f"-I{config_dir}",
                "-c",
                "-o",
                f"{obj_file.absolute()}",
                f"{src_file.absolute()}",
            ],
            "directory": f"{champsim_dir}",
            "file": f"{src_file.absolute()}",
            "output": f"{obj_file.absolute()}",
        })


    # Save the compile_commands.json file
    with open(module_dir / "compile_commands.json", "wt", encoding="utf-8") as f:
        f.write(json.dumps(entries, indent=2))


def main():
    """Generate a compile_commands.json file for a ChampSim module."""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--module-dir",
        type=Path,
        required=True,
        help="The path to the module to create a compile commands file for",
    )
    parser.add_argument(
        "--config-dir",
        type=Path,
        help="The path to the ChampSim config directory",
        default=DEFAULT_CONFIG_DIR,
    )

    args = parser.parse_args()
    create_module_compile_commands(
        module_dir=args.module_dir,
        config_dir=args.config_dir,
    )


if __name__ == "__main__":
    main()
