#!/usr/bin/env python3

"""Generate a compile_commands.json file for the ChampSim headers."""

import argparse
import json
import os
from pathlib import Path
from typing import Any, Dict, Final, List

from common import DEFAULT_CONFIG_DIR, DEFAULT_CHAMPSIM_DIR, get_options


def create_inc_compile_commands(
    champsim_dir: Path = DEFAULT_CHAMPSIM_DIR,
    config_dir: Path = DEFAULT_CONFIG_DIR,
) -> None:
    """Create the compile_commands.json file for the ChampSim header files.

    :param champsim_dir: The path to the ChampSim repository
    :param config_dir: The path to the ChampSim config directory
    """
    inc_dir: Final[Path] = champsim_dir / "inc"
    inc_files: Final[List[Path]] = list(inc_dir.glob("**/*.h"))
    entries: List[Dict[str, Any]] = []

    for inc_file in inc_files:
        entries.append({
            "arguments": [
                os.environ.get("CXX", "g++"),
                *get_options(champsim_dir / "global.options"),
                *get_options(champsim_dir / "absolute.options"),
                f"-I{config_dir}",
            ],
            "directory": f"{champsim_dir.absolute()}",
            "file": f"{inc_file.absolute()}",
        })

    # Save the compile_commands.json file
    with open(inc_dir / "compile_commands.json", "wt", encoding="utf-8") as f:
        f.write(json.dumps(entries, indent=2))

def main():
    """Generate a compile_commands.json file for the ChampSim heeader files."""
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
    
    create_inc_compile_commands(
        champsim_dir=args.champsim_dir,
        config_dir=args.config_dir,
    )


if __name__ == "__main__":
    main()
