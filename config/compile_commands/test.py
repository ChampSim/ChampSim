#!/usr/bin/env python3

"""Generate a compile_commands.json file for the ChampSim test code."""

import argparse
import json
import os
from pathlib import Path
from typing import Any, Dict, List, Final

from common import DEFAULT_CHAMPSIM_DIR, DEFAULT_CONFIG_DIR, get_options


def create_test_compile_commands(
    champsim_dir: Path = DEFAULT_CHAMPSIM_DIR,
    config_dir: Path = DEFAULT_CONFIG_DIR,
) -> None:
    """Create the compile_commands.json file for the ChampSim test files.
    
    :param champsim_dir: The path to the ChampSim repository
    :param config_dir: The path to the ChampSim config directory
    """
    test_dir: Final[Path] = champsim_dir / "test" / "cpp" / "src"
    src_files: Final[List[Path]] = list(test_dir.glob("**/*.cc"))
    entries: List[Dict[str, Any]] = []

    for src_file in src_files:
        obj_file: Path = (
            config_dir
            / "test"
            / src_file.relative_to(test_dir).with_suffix(".o")
        )
        entries.append({
            "arguments": [
                os.environ.get("CXX", "g++"),
                *get_options(champsim_dir / "global.options"),
                *get_options(champsim_dir / "absolute.options"),
                "-I.csconfig",
                "-I",
                ".",
                "-DCHAMPSIM_TEST_BUILD",
                "-g3",
                "-Og",
                "-c",
                "-o",
                f"{obj_file.absolute()}",
                f"{src_file.absolute()}",
            ],
            "directory": f"{champsim_dir.absolute()}",
            "file": f"{src_file.absolute()}",
            "output": f"{obj_file.absolute()}",
        })

    with open(test_dir / "compile_commands.json", "wt", encoding="utf-8") as f:
        json.dump(entries, f, indent=4)


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

    args = parser.parse_args()

    create_test_compile_commands(
        champsim_dir=args.champsim_dir,
        config_dir=args.config_dir
    )


if __name__ == "__main__":
    main()
