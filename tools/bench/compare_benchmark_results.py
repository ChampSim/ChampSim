#!/usr/bin/env python3
"""Compare normalized benchmark reports and emit drift summaries."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any


TIME_UNIT_TO_NS = {"ns": 1.0, "us": 1_000.0, "ms": 1_000_000.0, "s": 1_000_000_000.0}
VERDICT_ORDER = {"fail": 0, "error": 1, "warn": 2, "missing": 3, "new": 4, "pass": 5}


class BenchmarkCompareError(ValueError):
    """Raised when benchmark reports cannot be compared."""


def _read_json(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise BenchmarkCompareError(f"{path} is not valid JSON: {exc}") from exc
    except OSError as exc:
        raise BenchmarkCompareError(f"could not read {path}: {exc}") from exc

    if not isinstance(data, dict):
        raise BenchmarkCompareError(f"{path} must contain a JSON object")
    return data


def _benchmark_map(report: dict[str, Any], path: Path, metric: str) -> dict[str, dict[str, Any]]:
    if report.get("schema_version") != 1:
        raise BenchmarkCompareError(f"{path} has unsupported schema_version {report.get('schema_version')!r}")

    benchmarks = report.get("benchmarks")
    if not isinstance(benchmarks, list):
        raise BenchmarkCompareError(f"{path} is missing a benchmark list")

    mapped: dict[str, dict[str, Any]] = {}
    for index, benchmark in enumerate(benchmarks):
        if not isinstance(benchmark, dict):
            raise BenchmarkCompareError(f"{path} benchmark at index {index} must be an object")

        name = benchmark.get("name")
        if not isinstance(name, str) or not name:
            raise BenchmarkCompareError(f"{path} benchmark at index {index} is missing a non-empty name")
        if name in mapped:
            raise BenchmarkCompareError(f"{path} contains duplicate benchmark name {name!r}")
        if metric not in benchmark:
            raise BenchmarkCompareError(f"{path} benchmark {name!r} is missing metric {metric!r}")

        mapped[name] = benchmark

    return mapped


def _metric_value(benchmark: dict[str, Any], metric: str, report_label: str) -> float:
    value = benchmark.get(metric)
    name = benchmark.get("name", "<unknown>")
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise BenchmarkCompareError(f"{report_label} benchmark {name!r} metric {metric!r} must be numeric")
    parsed = float(value)
    if not math.isfinite(parsed) or parsed < 0:
        raise BenchmarkCompareError(f"{report_label} benchmark {name!r} metric {metric!r} must be finite and non-negative")
    return parsed


def _unit(benchmark: dict[str, Any], report_label: str) -> str:
    unit = benchmark.get("unit", "unknown")
    name = benchmark.get("name", "<unknown>")
    if not isinstance(unit, str) or not unit:
        raise BenchmarkCompareError(f"{report_label} benchmark {name!r} has invalid unit {unit!r}")
    return unit


def _common_values(
    baseline: dict[str, Any],
    current: dict[str, Any],
    metric: str,
) -> tuple[float, float, str]:
    baseline_unit = _unit(baseline, "baseline")
    current_unit = _unit(current, "current")
    baseline_value = _metric_value(baseline, metric, "baseline")
    current_value = _metric_value(current, metric, "current")

    if baseline_unit in TIME_UNIT_TO_NS and current_unit in TIME_UNIT_TO_NS:
        return baseline_value * TIME_UNIT_TO_NS[baseline_unit], current_value * TIME_UNIT_TO_NS[current_unit], "ns"

    if baseline_unit == current_unit == "unknown":
        return baseline_value, current_value, "unknown"

    raise BenchmarkCompareError(f"cannot compare units {baseline_unit!r} and {current_unit!r}")


def _delta_percent(delta: float, baseline: float) -> float | None:
    if baseline == 0:
        return 0.0 if delta == 0 else None
    return (delta / baseline) * 100.0


def _verdict(
    delta: float,
    delta_percent: float | None,
    *,
    warn_threshold_percent: float,
    fail_threshold_percent: float,
    min_absolute_change: float,
) -> str:
    if delta <= 0 or abs(delta) < min_absolute_change:
        return "pass"
    if delta_percent is None:
        return "fail"
    if delta_percent >= fail_threshold_percent:
        return "fail"
    if delta_percent >= warn_threshold_percent:
        return "warn"
    return "pass"


def compare_reports(
    baseline_report: dict[str, Any],
    current_report: dict[str, Any],
    *,
    baseline_path: Path = Path("<baseline>"),
    current_path: Path = Path("<current>"),
    metric: str = "mean",
    warn_threshold_percent: float = 5.0,
    fail_threshold_percent: float = 10.0,
    min_absolute_change: float = 0.0,
    sort: str = "name",
) -> dict[str, Any]:
    _validate_thresholds(warn_threshold_percent, fail_threshold_percent, min_absolute_change)
    baseline = _benchmark_map(baseline_report, baseline_path, metric)
    current = _benchmark_map(current_report, current_path, metric)
    names = sorted(set(baseline) | set(current))
    comparisons = []

    for name in names:
        if name not in baseline:
            comparisons.append(
                {
                    "name": name,
                    "baseline": None,
                    "current": _metric_value(current[name], metric, "current"),
                    "unit": _unit(current[name], "current"),
                    "baseline_unit": None,
                    "current_unit": _unit(current[name], "current"),
                    "delta": None,
                    "delta_percent": None,
                    "verdict": "new",
                }
            )
            continue

        if name not in current:
            comparisons.append(
                {
                    "name": name,
                    "baseline": _metric_value(baseline[name], metric, "baseline"),
                    "current": None,
                    "unit": _unit(baseline[name], "baseline"),
                    "baseline_unit": _unit(baseline[name], "baseline"),
                    "current_unit": None,
                    "delta": None,
                    "delta_percent": None,
                    "verdict": "missing",
                }
            )
            continue

        try:
            baseline_value, current_value, unit = _common_values(baseline[name], current[name], metric)
            delta = current_value - baseline_value
            delta_pct = _delta_percent(delta, baseline_value)
            unbounded_regression = baseline_value == 0 and delta > 0
            verdict = _verdict(
                delta,
                delta_pct,
                warn_threshold_percent=warn_threshold_percent,
                fail_threshold_percent=fail_threshold_percent,
                min_absolute_change=min_absolute_change,
            )
        except BenchmarkCompareError as exc:
            comparisons.append(
                {
                    "name": name,
                    "baseline": _metric_value(baseline[name], metric, "baseline"),
                    "current": _metric_value(current[name], metric, "current"),
                    "unit": f"{_unit(baseline[name], 'baseline')} -> {_unit(current[name], 'current')}",
                    "baseline_unit": _unit(baseline[name], "baseline"),
                    "current_unit": _unit(current[name], "current"),
                    "delta": None,
                    "delta_percent": None,
                    "verdict": "error",
                    "error": str(exc),
                }
            )
            continue

        comparisons.append(
            {
                "name": name,
                "baseline": baseline_value,
                "current": current_value,
                "unit": unit,
                "baseline_unit": _unit(baseline[name], "baseline"),
                "current_unit": _unit(current[name], "current"),
                "delta": delta,
                "delta_percent": delta_pct,
                "verdict": verdict,
            }
        )
        if unbounded_regression:
            comparisons[-1]["unbounded_regression"] = True

    comparisons = _sort_comparisons(comparisons, sort)
    summary = {"pass": 0, "warn": 0, "fail": 0, "new": 0, "missing": 0, "error": 0}
    for comparison in comparisons:
        summary[comparison["verdict"]] += 1

    return {"schema_version": 1, "summary": summary, "comparisons": comparisons}


def _validate_thresholds(warn_threshold_percent: float, fail_threshold_percent: float, min_absolute_change: float) -> None:
    for label, value in (
        ("warn-threshold-percent", warn_threshold_percent),
        ("fail-threshold-percent", fail_threshold_percent),
        ("min-absolute-change", min_absolute_change),
    ):
        if not math.isfinite(value) or value < 0:
            raise BenchmarkCompareError(f"{label} must be finite and non-negative")
    if fail_threshold_percent < warn_threshold_percent:
        raise BenchmarkCompareError("fail-threshold-percent must be greater than or equal to warn-threshold-percent")


def _sort_comparisons(comparisons: list[dict[str, Any]], sort: str) -> list[dict[str, Any]]:
    if sort == "name":
        return sorted(comparisons, key=lambda row: row["name"])
    if sort == "verdict":
        return sorted(comparisons, key=lambda row: (VERDICT_ORDER[row["verdict"]], row["name"]))
    if sort == "change":
        return sorted(
            comparisons,
            key=lambda row: (-_change_sort_value(row), row["name"]),
        )
    raise BenchmarkCompareError(f"unknown sort mode {sort!r}")


def _change_sort_value(row: dict[str, Any]) -> float:
    if row.get("unbounded_regression"):
        return float("inf")
    if row["delta_percent"] is None:
        return float("-inf")
    return row["delta_percent"]


def write_json(report: dict[str, Any], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, allow_nan=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _display_number(value: Any) -> str:
    if value is None:
        return "n/a"
    if isinstance(value, float):
        return f"{value:.6g}"
    return str(value)


def _display_percent(value: Any) -> str:
    if value is None:
        return "n/a"
    return f"{value:+.2f}%"


def _markdown_cell(value: Any) -> str:
    return str(value).replace("\\", "\\\\").replace("\r", " ").replace("\n", " ").replace("\t", " ").replace("|", "\\|")


def markdown_report(report: dict[str, Any], *, warn_threshold_percent: float, fail_threshold_percent: float) -> str:
    summary = report["summary"]
    total = len(report["comparisons"])
    lines = [
        "# Benchmark Drift Report",
        "",
        f"Benchmarks compared: {total}",
        f"Thresholds: warn >= {warn_threshold_percent:g}%, fail >= {fail_threshold_percent:g}% regression",
        "",
        "| Verdict | Count |",
        "| --- | ---: |",
    ]

    for verdict in ("fail", "warn", "pass", "new", "missing", "error"):
        lines.append(f"| {verdict} | {summary.get(verdict, 0)} |")

    regressions = [row for row in report["comparisons"] if row["verdict"] in {"fail", "warn"}]
    regressions.sort(key=lambda row: (VERDICT_ORDER[row["verdict"]], -_change_sort_value(row), row["name"]))
    if regressions:
        lines.extend(["", "## Worst Regressions", ""])
        for row in regressions[:5]:
            pct = _display_percent(row["delta_percent"])
            lines.append(f"- {_markdown_cell(row['name'])}: {pct} ({_display_number(row['delta'])} {row['unit']})")

    lines.extend(
        [
            "",
            "## Details",
            "",
            "| Benchmark | Baseline | Current | Unit | Delta | Delta % | Verdict |",
            "| --- | ---: | ---: | --- | ---: | ---: | --- |",
        ]
    )

    for row in report["comparisons"]:
        lines.append(
            "| {name} | {baseline} | {current} | {unit} | {delta} | {delta_percent} | {verdict} |".format(
                name=_markdown_cell(row["name"]),
                baseline=_display_number(row["baseline"]),
                current=_display_number(row["current"]),
                unit=_markdown_cell(row["unit"]),
                delta=_display_number(row["delta"]),
                delta_percent=_display_percent(row["delta_percent"]),
                verdict=row["verdict"],
            )
        )

    return "\n".join(lines) + "\n"


def write_markdown(report: dict[str, Any], path: Path, *, warn_threshold_percent: float, fail_threshold_percent: float) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        markdown_report(report, warn_threshold_percent=warn_threshold_percent, fail_threshold_percent=fail_threshold_percent),
        encoding="utf-8",
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Compare normalized benchmark JSON reports.")
    parser.add_argument("--baseline", required=True, type=Path, help="Path to baseline normalized benchmark JSON.")
    parser.add_argument("--current", required=True, type=Path, help="Path to current normalized benchmark JSON.")
    parser.add_argument("--output-md", required=True, type=Path, help="Path for the Markdown comparison report.")
    parser.add_argument("--output-json", type=Path, help="Optional path for machine-readable JSON comparison output.")
    parser.add_argument("--warn-threshold-percent", type=float, default=5.0, help="Warn when runtime regresses by this percent.")
    parser.add_argument("--fail-threshold-percent", type=float, default=10.0, help="Fail verdict when runtime regresses by this percent.")
    parser.add_argument("--min-absolute-change", type=float, default=0.0, help="Ignore smaller absolute runtime changes.")
    parser.add_argument("--metric", default="mean", help="Normalized benchmark metric to compare.")
    parser.add_argument("--sort", choices=("name", "change", "verdict"), default="name", help="Comparison row ordering.")
    parser.add_argument(
        "--fail-on-regression",
        action="store_true",
        help="Exit nonzero on fail, error, or missing verdicts. Default is report-only for artifact workflows.",
    )
    return parser


def exit_code(report: dict[str, Any], *, fail_on_regression: bool) -> int:
    if not fail_on_regression:
        return 0
    summary = report["summary"]
    if summary["fail"] or summary["error"] or summary["missing"]:
        return 2
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        report = compare_reports(
            _read_json(args.baseline),
            _read_json(args.current),
            baseline_path=args.baseline,
            current_path=args.current,
            metric=args.metric,
            warn_threshold_percent=args.warn_threshold_percent,
            fail_threshold_percent=args.fail_threshold_percent,
            min_absolute_change=args.min_absolute_change,
            sort=args.sort,
        )
        write_markdown(
            report,
            args.output_md,
            warn_threshold_percent=args.warn_threshold_percent,
            fail_threshold_percent=args.fail_threshold_percent,
        )
        if args.output_json is not None:
            write_json(report, args.output_json)
    except BenchmarkCompareError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return exit_code(report, fail_on_regression=args.fail_on_regression)


if __name__ == "__main__":
    sys.exit(main())
