#!/usr/bin/env python3

"""Generate a compile_commands.json file for the ChampSim source code."""

import argparse
import json
import os
from pathlib import Path
from typing import Any, Dict, Final, List

from common import DEFAULT_CONFIG_DIR, DEFAULT_CHAMPSIM_DIR, get_options


def create_src_compile_commands(
    build_id: str,
    champsim_dir: Path = DEFAULT_CHAMPSIM_DIR,
    config_dir: Path = DEFAULT_CONFIG_DIR,
) -> None:
    """Create the compile_commands.json file for the ChampSim source files.

    :param build_id: The build ID
    :param champsim_dir: The path to the ChampSim repository
    :param config_dir: The path to the ChampSim config directory
    """
    src_dir: Final[Path] = champsim_dir / "src"
    src_files: Final[List[Path]] = list(src_dir.glob("**/*.cc"))
    entries: List[Dict[str, Any]] = []

    for src_file in src_files:
        if src_file.parts[-1] == "main.cc":
            # main.cc is a special case
            obj_file: Path = config_dir / f"{build_id}_main.o"
            entries.append({
                "arguments": [
                    os.environ.get("CXX", "g++"),
                    *get_options(champsim_dir / "global.options"),
                    *get_options(champsim_dir / "absolute.options"),
                    "-I.csconfig",
                    f"-DCHAMPSIM_BUILD=0x{build_id}",
                    "-c",
                    "-o",
                    f"{obj_file.absolute()}",
                    f"{src_file.absolute()}",
                ],
                "directory": f"{champsim_dir.absolute()}",
                "file": f"{src_file.absolute()}",
                "output": f"{obj_file.absolute()}",
            })
        else:
            obj_file: Path = (
                config_dir
                / src_file.relative_to(src_dir).with_suffix(".o")
            )
            entries.append({
                "arguments": [
                    os.environ.get("CXX", "g++"),
                    *get_options(champsim_dir / "global.options"),
                    *get_options(champsim_dir / "absolute.options"),
                    "-I.csconfig",
                    "-c",
                    "-o",
                    f"{obj_file.absolute()}",
                    f"{src_file.absolute()}",
                ],
                "directory": f"{champsim_dir.absolute()}",
                "file": f"{src_file.absolute()}",
                "output": f"{obj_file.absolute()}",
            })

    # Save the compile_commands.json file
    with open(src_dir / "compile_commands.json", "wt", encoding="utf-8") as f:
        f.write(json.dumps(entries, indent=2))

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

    args = parser.parse_args()
    
    create_src_compile_commands(
        build_id=args.build_id,
        champsim_dir=args.champsim_dir,
        config_dir=args.config_dir,
    )


if __name__ == "__main__":
    main()
