"""Common default values and functions for generating compile_commands.json files."""

import json
from pathlib import Path
from typing import Any, Callable, Dict, Final, List, Optional

DEFAULT_CHAMPSIM_DIR: Final[Path] = Path(".")
DEFAULT_CONFIG_DIR: Final[Path] = Path(".csconfig")
DEFAULT_INDENT: Final[int] = 2


def get_options(options_file: Path) -> List[str]:
    """Read the compile options from a .options file.

    :param options_file: Path to the .options file.
    :return List of compile options.
    """
    with options_file.open() as f:
        return f.read().split()


def get_files(directory: Path, extensions: List[str]) -> List[Path]:
    """Get all the files matching a list of extensions from a directory.

    :param source_dir: Directory to recursively search for source files.
    :param extensions: List of file extensions to search for.
    :return List of source files.
    """
    return [file for ext in extensions for file in directory.glob(f"**/*.{ext}")]


class CompileCommand:
    """A single compile command for a compile_commands.json file."""

    def __init__(
        self,
        arguments: List[str],
        directory: Optional[Path] = None,
        file: Optional[Path] = None,
        output: Optional[Path] = None,
    ) -> None:
        """Initialize the compile command.

        :param arguments: List of arguments for the compile command.
        :param directory: Directory to run the command in.
        :param file: File to compile.
        :param output: Output file to create.
        """
        self.arguments: Final[List[str]] = arguments
        self.directory: Final[Optional[Path]] = directory
        self.file: Final[Optional[Path]] = file
        self.output: Final[Optional[Path]] = output

    def to_dict(self) -> Dict[str, Any]:
        """Convert the compile command to a dictionary.

        :return Dictionary representing the compile command.
        """
        dic: Dict[str, Any] = {}
        dic["arguments"] = self.arguments
        if self.directory:
            dic["directory"] = self.directory.absolute()
        if self.file:
            dic["file"] = self.file.absolute()
        if self.output:
            dic["output"] = self.output.absolute()
        return dic


class CompileCommandManifest:
    """A manifest of compile commands for a compile_commands.json file."""

    def __init__(self, compile_commands_file: Path) -> None:
        """Initialize the manifest.

        :param compile_commands_file: Path to the compile_commands.json file.
        """
        self.compile_commands_file: Final[Path] = compile_commands_file
        self.entries: List[CompileCommand] = []

    @staticmethod
    def Create(
        directory: Path,
        extensions: List[str],
        create_fn: Callable,
        **kwargs,
    ) -> "CompileCommandManifest":
        """Create a manifest using pre-defined functions.

        :param compile_commands_file: Path to the compile_commands.json file.
        :param directory: Directory to search for files.
        :param extensions: List of file extensions to search for.
        :param create_fn: Function to create a single compile command for one source file.
        :param kwargs: Keyword arguments for <create_fn>.
        """
        directory_: Final[Path] = directory.absolute()
        manifest = CompileCommandManifest(directory_ / "compile_commands.json")
        files: Final[List[Path]] = get_files(directory_, extensions)
        for file in files:
            manifest.append(create_fn(file, **kwargs))
        return manifest

    def append(self, entry: CompileCommand) -> None:
        """Add a compile command to the manifest.

        :param entry: Compile command to add.
        """
        self.entries.append(entry)

    def to_json(self, indent: int = DEFAULT_INDENT) -> str:
        """Convert the manifest to a JSON string.

        :param indent: Number of spaces to indent the JSON string.
        :return JSON string representing the manifest.
        """
        entries_: List[Dict[str, Any]] = [e.to_dict() for e in self.entries]
        entries_ = [
            {k: str(v.absolute()) if isinstance(v, Path) else v for k, v in e.items()}
            for e in entries_
        ]
        return json.dumps(entries_, indent=indent)

    def save(self, indent: int = DEFAULT_INDENT) -> None:
        """Save the compile commands to the compile_commands.json file.

        :param indent: Number of spaces to indent the JSON file.
        """
        self.compile_commands_file.write_text(self.to_json(indent=indent))
