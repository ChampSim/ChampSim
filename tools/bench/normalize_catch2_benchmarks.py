#!/usr/bin/env python3
"""Normalize Catch2 XML benchmark reports for CI artifacts."""

from __future__ import annotations

import argparse
import json
import math
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


VALID_UNITS = {"ns", "us", "ms", "s", "unknown"}
UNIT_ALIASES = {
    "nanosecond": "ns",
    "nanoseconds": "ns",
    "microsecond": "us",
    "microseconds": "us",
    "millisecond": "ms",
    "milliseconds": "ms",
    "second": "s",
    "seconds": "s",
}


class BenchmarkParseError(ValueError):
    """Raised when a Catch2 benchmark report cannot be normalized."""


def _local_name(tag: str) -> str:
    return tag.rsplit("}", maxsplit=1)[-1]


def _child(element: ET.Element, name: str) -> ET.Element | None:
    for child in element:
        if _local_name(child.tag) == name:
            return child
    return None


def _required_float(element: ET.Element, attr: str, context: str) -> float:
    value = element.get(attr)
    if value is None:
        raise BenchmarkParseError(f"{context} is missing required attribute {attr!r}")
    try:
        parsed = float(value)
    except ValueError as exc:
        raise BenchmarkParseError(f"{context} has non-numeric {attr!r}: {value!r}") from exc
    if not math.isfinite(parsed) or parsed < 0:
        raise BenchmarkParseError(f"{context} has invalid {attr!r}: {value!r}")
    return parsed


def _optional_float(element: ET.Element | None, attr: str, context: str) -> float | None:
    if element is None or element.get(attr) is None:
        return None
    try:
        parsed = float(element.get(attr, ""))
    except ValueError as exc:
        raise BenchmarkParseError(f"{context} has non-numeric {attr!r}: {element.get(attr)!r}") from exc
    if not math.isfinite(parsed) or parsed < 0:
        raise BenchmarkParseError(f"{context} has invalid {attr!r}: {element.get(attr)!r}")
    return parsed


def _optional_int(element: ET.Element, attr: str, context: str) -> int | None:
    value = element.get(attr)
    if value is None:
        return None
    try:
        parsed = int(value)
    except ValueError as exc:
        raise BenchmarkParseError(f"{context} has non-integer {attr!r}: {value!r}") from exc
    if parsed < 0:
        raise BenchmarkParseError(f"{context} has invalid {attr!r}: {value!r}")
    return parsed


def _unit_for(element: ET.Element, context: str) -> str:
    unit = element.get("unit") or element.get("units") or "ns"
    normalized = UNIT_ALIASES.get(unit.strip().lower(), unit.strip().lower())
    if normalized not in VALID_UNITS:
        raise BenchmarkParseError(f"{context} has unsupported benchmark unit {unit!r}")
    return normalized


def _raw_fields(element: ET.Element, mean: ET.Element, stddev: ET.Element | None, context: str) -> dict[str, Any]:
    raw: dict[str, Any] = {}

    for attr in ("resamples", "clockResolution", "estimatedDuration"):
        if element.get(attr) is not None:
            raw[attr] = element.get(attr)

    if mean.get("ci") is not None:
        raw["mean_ci"] = _optional_float(mean, "ci", f"{context} mean")

    if stddev is not None:
        for attr, key in (
            ("lowerBound", "stddev_low"),
            ("upperBound", "stddev_high"),
            ("ci", "stddev_ci"),
        ):
            if stddev.get(attr) is not None:
                raw[key] = _optional_float(stddev, attr, f"{context} standardDeviation")

    outliers = _child(element, "outliers")
    if outliers is not None:
        raw["outliers"] = {
            key: outliers.get(key)
            for key in ("variance", "lowMild", "lowSevere", "highMild", "highSevere")
            if outliers.get(key) is not None
        }

    return raw


def normalize_benchmark(element: ET.Element) -> dict[str, Any]:
    name = (element.get("name") or "").strip()
    if not name:
        raise BenchmarkParseError("BenchmarkResults element is missing a non-empty name")

    context = f"benchmark {name!r}"
    failed = _child(element, "failed")
    if failed is not None:
        message = failed.get("message") or "unknown benchmark failure"
        raise BenchmarkParseError(f"benchmark {name!r} failed in Catch2 report: {message}")

    mean = _child(element, "mean")
    if mean is None:
        raise BenchmarkParseError(f"{context} is missing mean results")

    stddev = _child(element, "standardDeviation")
    unit = _unit_for(element, context)
    samples = _optional_int(element, "samples", context)
    iterations = _optional_int(element, "iterations", context)

    benchmark = {
        "name": name,
        "mean": _required_float(mean, "value", f"{context} mean"),
        "unit": unit,
        "low": _optional_float(mean, "lowerBound", f"{context} mean"),
        "high": _optional_float(mean, "upperBound", f"{context} mean"),
        "stddev": _optional_float(stddev, "value", f"{context} standardDeviation"),
        "samples": samples,
        "iterations": iterations,
        "raw": _raw_fields(element, mean, stddev, context),
    }

    return benchmark


def normalize_catch2_xml(path: Path, *, command: str = "", commit: str | None = None) -> dict[str, Any]:
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as exc:
        raise BenchmarkParseError(f"{path} is not valid XML: {exc}") from exc
    except OSError as exc:
        raise BenchmarkParseError(f"could not read {path}: {exc}") from exc

    elements = [element for element in root.iter() if _local_name(element.tag) == "BenchmarkResults"]
    if not elements:
        raise BenchmarkParseError(f"{path} does not contain Catch2 BenchmarkResults elements")

    benchmarks = []
    for element in elements:
        try:
            benchmarks.append(normalize_benchmark(element))
        except BenchmarkParseError as exc:
            raise BenchmarkParseError(f"{path}: {exc}") from exc

    seen_names: set[str] = set()
    for benchmark in benchmarks:
        if benchmark["name"] in seen_names:
            raise BenchmarkParseError(f"{path}: duplicate benchmark name {benchmark['name']!r}")
        seen_names.add(benchmark["name"])

    benchmarks.sort(key=lambda benchmark: benchmark["name"])

    source: dict[str, Any] = {"tool": "catch2", "command": command}
    if commit is not None:
        source["commit"] = commit

    return {"schema_version": 1, "source": source, "benchmarks": benchmarks}


def write_json(report: dict[str, Any], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, allow_nan=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _display(value: Any) -> str:
    if value is None:
        return "n/a"
    if isinstance(value, float):
        return f"{value:.6g}"
    return str(value)


def _markdown_cell(value: Any) -> str:
    return _display(value).replace("\\", "\\\\").replace("\r", " ").replace("\n", " ").replace("\t", " ").replace("|", "\\|")


def markdown_summary(report: dict[str, Any]) -> str:
    lines = [
        "# Catch2 Benchmark Summary",
        "",
        f"Benchmarks: {len(report['benchmarks'])}",
        "",
        "| Benchmark | Mean | Unit | Low | High | Stddev | Samples | Iterations |",
        "| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |",
    ]

    for benchmark in report["benchmarks"]:
        lines.append(
            "| {name} | {mean} | {unit} | {low} | {high} | {stddev} | {samples} | {iterations} |".format(
                name=_markdown_cell(benchmark["name"]),
                mean=_markdown_cell(benchmark["mean"]),
                unit=_markdown_cell(benchmark["unit"]),
                low=_markdown_cell(benchmark["low"]),
                high=_markdown_cell(benchmark["high"]),
                stddev=_markdown_cell(benchmark["stddev"]),
                samples=_markdown_cell(benchmark["samples"]),
                iterations=_markdown_cell(benchmark["iterations"]),
            )
        )

    return "\n".join(lines) + "\n"


def write_markdown(report: dict[str, Any], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(markdown_summary(report), encoding="utf-8")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Normalize Catch2 XML benchmark output into stable JSON and optional Markdown."
    )
    parser.add_argument("--input", required=True, type=Path, help="Path to a Catch2 XML benchmark report.")
    parser.add_argument("--output", required=True, type=Path, help="Path for normalized benchmark JSON.")
    parser.add_argument("--markdown", type=Path, help="Optional path for a Markdown summary table.")
    parser.add_argument("--strict", action="store_true", help="Accepted for compatibility; XML input is always strict.")
    parser.add_argument("--command", default="", help="Benchmark command to record in the JSON source metadata.")
    parser.add_argument("--commit", help="Optional commit SHA to record in the JSON source metadata.")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        report = normalize_catch2_xml(args.input, command=args.command, commit=args.commit)
        write_json(report, args.output)
        if args.markdown is not None:
            write_markdown(report, args.markdown)
    except BenchmarkParseError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
