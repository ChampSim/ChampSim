import glob
import json
import os
from typing import Any, Dict, Generator, List


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


def get_champsim_root():
    """Get the ChampSim root directory."""
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def get_module_entries(
    module: Dict[str, Any],
) -> Generator[Dict[str, Any], None, None]:
    """Get the compile command for each source file in a module.

    :param module_info: The info about the module.
    """
    champsim_root = get_champsim_root()

    module_dir = os.path.relpath(module["path"], start=champsim_root)
    module_src_dir = os.path.join(champsim_root, module_dir)
    module_src_files = glob.glob(
        os.path.join(module_src_dir, "**/*.cc"), recursive=True
    )

    for src_file in module_src_files:
        obj_file = os.path.join(".csconfig", src_file.replace(".cc", ".o"))
        entry = {
            "arguments": [
                os.environ.get("CXX", "g++"),
                *get_options("global.options"),
                *get_options("absolute.options"),
                *get_options("module.options"),
                "-I.csconfig",
                "-c",
                "-o",
                obj_file,
            ],
            "directory": champsim_root,
            "file": src_file,
            "output": obj_file,
        }

        yield entry


def get_main_entry(build_id: str) -> Dict[str, Any]:
    """Get the entry for main.cc.

    :param build_id: The build ID
    """
    champsim_root = get_champsim_root()
    src_file = os.path.join("src", "main.cc")
    obj_file = os.path.join(".csconfig", f"{build_id}_main.o")

    return {
        "arguments": [
            os.environ.get("CXX", "g++"),
            *get_options("global.options"),
            *get_options("absolute.options"),
            "-I.csconfig",
            f"-DCHAMPSIM_BUILD=0x{build_id}",
            "-c",
            "-o",
            obj_file,
            src_file,
        ],
        "directory": champsim_root,
        "file": src_file,
        "output": obj_file,
    }


def get_champsim_entries(build_id: str) -> Generator[Dict[str, Any], None, None]:
    """Get the non-module compile commands for each source file.

    :param build_id: The build ID
    """
    champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    champsim_src_dir = os.path.join(champsim_root, "src")
    champsim_src_files = glob.glob(os.path.join(champsim_src_dir, "*.cc"))

    for src_file in champsim_src_files:
        src_file = os.path.relpath(src_file, start=champsim_root)
        obj_file = os.path.basename(src_file).replace(".cc", ".o")

        if src_file.endswith("main.cc"):
            yield get_main_entry(build_id)
        else:
            yield {
                "arguments": [
                    os.environ.get("CXX", "g++"),
                    *get_options("global.options"),
                    *get_options("absolute.options"),
                    "-I.csconfig",
                    "-c",
                    "-o",
                    os.path.join(".csconfig", obj_file),
                    src_file,
                ],
                "directory": champsim_root,
                "file": src_file,
                "output": obj_file,
            }


def get_compile_commands_lines(
    build_id: str,
    module_info: Dict[str, Any],
) -> Generator[str, None, None]:
    """Generate the lines to be written for a configuration's compile_commands.json.

    :param build_id: The build ID
    :param executable: The executable
    :param module_info: The module information
    """
    entries: List[Dict[str, Any]] = []

    # Get ChampSim file entries
    for entry in get_champsim_entries(build_id):
        entries.append(entry)

    # Get module file entries
    for module in module_info.values():
        for entry in get_module_entries(module):
            entries.append(entry)

    # Dump lines as JSON
    for line in json.dumps(entries, indent=2).split("\n"):
        yield line
