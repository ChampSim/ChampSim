#!/usr/bin/env python3

"""Script for generating compile_commands.json files."""

import argparse
import glob
import json
import os
from pathlib import Path
from typing import Any, Dict, Final, Generator, List


def get_options(options_file: str) -> List[str]:
    """Read an options file.

    :param options_file: The options file
    :return options: The options in the file
    """
    with open(options_file, "rt", encoding="utf-8") as f:
        options = []
        for line in f:
            options.extend(line.strip().split(" "))

    return options


def get_champsim_root() -> Path:
    """Get the ChampSim root directory."""
    return Path(__file__).resolve().absolute().parent.parent


def get_main_entry(build_id: str) -> Dict[str, Any]:
    """Get the entry for main.cc.

    :param build_id: The build ID
    """
    champsim_root: Final[Path] = get_champsim_root()
    main_src_file: Final[Path] = champsim_root / "src" / "main.cc"
    main_obj_file: Final[Path] = champsim_root / ".csconfig" / f"{build_id}_main.o"

    return {
        "arguments": [
            os.environ.get("CXX", "g++"),
            "@global.options",  # *get_options("global.options"),
            "@absolute.options",  # *get_options("absolute.options"),
            "-I.csconfig",
            f"-DCHAMPSIM_BUILD=0x{build_id}",
            "-c",
            "-o",
            f"{main_obj_file}",
            f"{main_src_file}",
        ],
        "directory": f"{champsim_root}",
        "file": f"{main_src_file}",
        "output": f"{main_obj_file}",
    }


def get_src_entries(build_id: str) -> Generator[Dict[str, Any], None, None]:
    """Get the non-module compile commands for each source file.

    :param build_id: The build ID
    """
    champsim_root: Final[Path] = get_champsim_root()
    champsim_src_dir: Final[Path] = champsim_root / "src"
    champsim_src_files: Final[List[Path]] = list(champsim_src_dir.glob("**/*.cc"))

    # print(f"ChampSim root      : {champsim_root}")
    # print(f"ChampSim src dir   : {champsim_src_dir}")
    # print(f"ChampSim src files : {champsim_src_files}")

    for champsim_src_file in champsim_src_files:
        champsim_obj_file: Path = (
            champsim_root
            / ".csconfig"
            / champsim_src_file.relative_to(champsim_root / "src").with_suffix(".o")
        )
        # print(f"{champsim_src_file}")
        # print(f"{champsim_obj_file}")

        if champsim_src_file.parts[-1] == "main.cc":
            yield get_main_entry(build_id)
        else:
            yield {
                "arguments": [
                    os.environ.get("CXX", "g++"),
                    "@global.options",  # *get_options("global.options"),
                    "@absolute.options",  # *get_options("absolute.options"),
                    "-I.csconfig",
                    "-c",
                    "-o",
                    f"{champsim_obj_file}",
                    f"{champsim_src_file}",
                ],
                "directory": f"{champsim_root}",
                "file": f"{champsim_src_file}",
                "output": f"{champsim_obj_file}",
            }


def get_module_entries(module: Path) -> Generator[Dict[str, Any], None, None]:
    """Get the compile command entries for a module's source files.

    :param module: The path to the module object file
    """
    champsim_root = get_champsim_root()

    module_obj_file = champsim_root / module
    module_src_dir = champsim_root / os.sep.join(module.parts[2:-1])
    module_src_files = list(module_src_dir.glob("**/*.cc"))

    # print(f"ChampSim root    : {champsim_root}")
    # print(f"Module           : {module}")
    # print(f"Module obj file  : {module_obj_file}")
    # print(f"Module src dir   : {module_src_dir}")
    # print(f"Module src files : {module_src_files}")

    for module_src_file in module_src_files:
        entry = {
            "arguments": [
                os.environ.get("CXX", "g++"),
                "@global.options",  # *get_options("global.options"),
                "@absolute.options",  # *get_options("absolute.options"),
                "@module.options",  # *get_options("module.options"),
                "-I.csconfig",
                "-c",
                "-o",
                f"{module_obj_file}",
                f"{module_src_file}",
            ],
            "directory": f"{champsim_root}",
            "file": f"{module_src_file}",
            "output": f"{module_obj_file}",
        }

        yield entry


def get_test_entries() -> Generator[Dict[str, Any], None, None]:
    """Get the test compile commands for each test source file."""
    champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    champsim_test_dir = os.path.join(champsim_root, "test", "cpp", "src")
    champsim_test_files = glob.glob(os.path.join(champsim_test_dir, "*.cc"))

    for test_file in champsim_test_files:
        src_file = os.path.relpath(test_file, start=champsim_root)
        obj_file = os.path.join(
            ".csconfig",
            "test",
            os.path.relpath(src_file, start="test/cpp/src").replace(".cc", ".o"),
        )

        yield {
            "arguments": [
                os.environ.get("CXX", "g++"),
                "@global.options",  # *get_options("global.options"),
                "@absolute.options",  # *get_options("absolute.options"),
                "-I.csconfig",
                "-I",
                ".",
                "-DCHAMPSIM_TEST_BUILD",
                "-g3",
                "-Og",
                "-c",
                "-o",
                obj_file,
                src_file,
            ],
            "directory": champsim_root,
            "file": src_file,
            "output": obj_file,
        }


def create_module_compile_commands(module: Path) -> None:
    """Create a module's compile_commands.json file.

    :param module: The path to the module object file
    """
    champsim_root = get_champsim_root()
    module_src_dir = champsim_root / os.sep.join(module.parts[2:-1])
    module_compile_commands_file = module_src_dir / "compile_commands.json"

    entries: List[Dict[str, Any]] = []

    for entry in get_module_entries(module):
        entries.append(entry)

    with open(module_compile_commands_file, "wt", encoding="utf-8") as f:
        f.write(json.dumps(entries, indent=2))


def create_src_compile_commands(build_id: str) -> None:
    """Create the compile_commands.json file for the ChampSim source files.

    :param build_id: The build ID
    """
    champsim_root: Final[Path] = get_champsim_root()
    champsim_src_dir: Final[Path] = champsim_root / "src"
    champsim_compile_commands_file: Final[Path] = (
        champsim_src_dir / "compile_commands.json"
    )

    entries: List[Dict[str, Any]] = []

    for entry in get_src_entries(build_id):
        entries.append(entry)

    with open(champsim_compile_commands_file, "wt", encoding="utf-8") as f:
        f.write(json.dumps(entries, indent=2))


def create_test_compile_commands() -> None:
    """Create the compile_commands.json file for the ChampSim test files."""
    champsim_root = get_champsim_root()
    champsim_test_dir = champsim_root / "test" / "cpp" / "src"
    champsim_compile_commands_file = champsim_test_dir / "compile_commands.json"

    entries: List[Dict[str, Any]] = []

    for entry in get_test_entries():
        entries.append(entry)

    with open(champsim_compile_commands_file, "wt", encoding="utf-8") as f:
        f.write(json.dumps(entries, indent=2))


def main():
    """Generate a compile_commands.json file."""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-id",
        type=str,
        help="The build ID",
        required=True,
    )
    parser.add_argument(
        "--modules",
        type=Path,
        nargs="+",
        help="The path to each module object file to include in the compile commands",
    )

    args = parser.parse_args()
    build_id: Final[str] = args.build_id
    modules: Final[List[Path]] = args.modules

    # print(f"Build ID : {build_id}")
    # print(f"Modules  : {modules}")

    for module in modules:
        create_module_compile_commands(module)

    create_src_compile_commands(build_id)
    create_test_compile_commands()


if __name__ == "__main__":
    main()
