import contextlib
import importlib.util
import io
import json
import pathlib
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools" / "bench" / "compare_benchmark_results.py"
FIXTURES = ROOT / "test" / "python" / "fixtures" / "benchmark_diff"


def load_tool():
    spec = importlib.util.spec_from_file_location("compare_benchmark_results", TOOL)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


diff_tool = load_tool()


def load_fixture(name):
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))


class BenchmarkDiffTests(unittest.TestCase):
    def test_pass_warn_fail_improvement_new_and_missing(self):
        report = diff_tool.compare_reports(load_fixture("baseline.json"), load_fixture("current.json"))

        by_name = {row["name"]: row for row in report["comparisons"]}
        self.assertEqual(by_name["alpha pass"]["verdict"], "pass")
        self.assertEqual(by_name["beta warn"]["verdict"], "warn")
        self.assertEqual(by_name["gamma fail"]["verdict"], "fail")
        self.assertEqual(by_name["delta improves"]["verdict"], "pass")
        self.assertEqual(by_name["epsilon missing"]["verdict"], "missing")
        self.assertEqual(by_name["theta new"]["verdict"], "new")
        self.assertEqual(report["summary"], {"pass": 2, "warn": 1, "fail": 3, "new": 1, "missing": 1, "error": 0})

    def test_zero_baseline_is_safe(self):
        report = diff_tool.compare_reports(load_fixture("baseline.json"), load_fixture("current.json"))
        zero = next(row for row in report["comparisons"] if row["name"] == "zeta zero")

        self.assertEqual(zero["baseline"], 0.0)
        self.assertEqual(zero["current"], 1.0)
        self.assertIsNone(zero["delta_percent"])
        self.assertTrue(zero["unbounded_regression"])
        self.assertEqual(zero["verdict"], "fail")

    def test_zero_to_zero_baseline_is_pass(self):
        baseline = {"schema_version": 1, "benchmarks": [{"name": "zero", "mean": 0.0, "unit": "ns"}]}
        current = {"schema_version": 1, "benchmarks": [{"name": "zero", "mean": 0.0, "unit": "ns"}]}

        report = diff_tool.compare_reports(baseline, current)

        self.assertEqual(report["comparisons"][0]["delta_percent"], 0.0)
        self.assertEqual(report["comparisons"][0]["verdict"], "pass")

    def test_unit_conversion_uses_common_nanoseconds(self):
        report = diff_tool.compare_reports(load_fixture("baseline.json"), load_fixture("current.json"))
        converted = next(row for row in report["comparisons"] if row["name"] == "eta unit conversion")

        self.assertEqual(converted["baseline"], 1000.0)
        self.assertEqual(converted["current"], 1100.0)
        self.assertEqual(converted["unit"], "ns")
        self.assertEqual(converted["baseline_unit"], "us")
        self.assertEqual(converted["current_unit"], "ns")
        self.assertEqual(converted["delta"], 100.0)
        self.assertEqual(converted["delta_percent"], 10.0)
        self.assertEqual(converted["verdict"], "fail")

    def test_unit_mismatch_is_reported_as_error(self):
        report = diff_tool.compare_reports(
            load_fixture("unit_mismatch_baseline.json"),
            load_fixture("unit_mismatch_current.json"),
        )

        self.assertEqual(report["summary"]["error"], 1)
        self.assertEqual(report["comparisons"][0]["verdict"], "error")
        self.assertIn("cannot compare units", report["comparisons"][0]["error"])

    def test_unknown_units_compare_only_to_unknown_units(self):
        baseline = {"schema_version": 1, "benchmarks": [{"name": "opaque", "mean": 1.0, "unit": "unknown"}]}
        current = {"schema_version": 1, "benchmarks": [{"name": "opaque", "mean": 1.5, "unit": "unknown"}]}

        report = diff_tool.compare_reports(baseline, current)

        self.assertEqual(report["comparisons"][0]["unit"], "unknown")
        self.assertEqual(report["comparisons"][0]["delta_percent"], 50.0)
        self.assertEqual(report["comparisons"][0]["verdict"], "fail")

    def test_duplicate_benchmark_names_raise(self):
        with self.assertRaisesRegex(diff_tool.BenchmarkCompareError, "duplicate benchmark name 'duplicate'"):
            diff_tool.compare_reports(load_fixture("duplicate_names.json"), load_fixture("current.json"))

    def test_empty_benchmark_lists_are_valid(self):
        report = diff_tool.compare_reports({"schema_version": 1, "benchmarks": []}, {"schema_version": 1, "benchmarks": []})

        self.assertEqual(report["summary"], {"pass": 0, "warn": 0, "fail": 0, "new": 0, "missing": 0, "error": 0})
        self.assertEqual(report["comparisons"], [])

    def test_schema_and_metric_shape_errors_are_explicit(self):
        current = {"schema_version": 1, "benchmarks": [{"name": "x", "mean": 1.0, "unit": "ns"}]}

        with self.assertRaisesRegex(diff_tool.BenchmarkCompareError, "unsupported schema_version"):
            diff_tool.compare_reports({"schema_version": 2, "benchmarks": []}, current)
        with self.assertRaisesRegex(diff_tool.BenchmarkCompareError, "missing metric 'mean'"):
            diff_tool.compare_reports({"schema_version": 1, "benchmarks": [{"name": "x", "unit": "ns"}]}, current)

    def test_invalid_metric_values_raise(self):
        for value in (True, float("nan"), float("inf"), -1.0):
            with self.subTest(value=value):
                baseline = {"schema_version": 1, "benchmarks": [{"name": "invalid", "mean": value, "unit": "ns"}]}
                current = {"schema_version": 1, "benchmarks": [{"name": "invalid", "mean": 1.0, "unit": "ns"}]}
                with self.assertRaisesRegex(diff_tool.BenchmarkCompareError, "must be"):
                    diff_tool.compare_reports(baseline, current)

    def test_threshold_validation(self):
        baseline = {"schema_version": 1, "benchmarks": [{"name": "x", "mean": 1.0, "unit": "ns"}]}
        current = {"schema_version": 1, "benchmarks": [{"name": "x", "mean": 1.0, "unit": "ns"}]}

        with self.assertRaisesRegex(diff_tool.BenchmarkCompareError, "fail-threshold-percent"):
            diff_tool.compare_reports(baseline, current, warn_threshold_percent=10.0, fail_threshold_percent=5.0)
        with self.assertRaisesRegex(diff_tool.BenchmarkCompareError, "min-absolute-change"):
            diff_tool.compare_reports(baseline, current, min_absolute_change=-1.0)

    def test_min_absolute_change_can_suppress_small_regressions(self):
        baseline = {"schema_version": 1, "benchmarks": [{"name": "small", "mean": 100.0, "unit": "ns"}]}
        current = {"schema_version": 1, "benchmarks": [{"name": "small", "mean": 109.0, "unit": "ns"}]}

        report = diff_tool.compare_reports(baseline, current, min_absolute_change=10.0)

        self.assertEqual(report["comparisons"][0]["verdict"], "pass")

    def test_exact_threshold_boundaries_are_inclusive(self):
        baseline = {
            "schema_version": 1,
            "benchmarks": [
                {"name": "warn boundary", "mean": 100.0, "unit": "ns"},
                {"name": "fail boundary", "mean": 100.0, "unit": "ns"},
            ],
        }
        current = {
            "schema_version": 1,
            "benchmarks": [
                {"name": "warn boundary", "mean": 105.0, "unit": "ns"},
                {"name": "fail boundary", "mean": 110.0, "unit": "ns"},
            ],
        }

        report = diff_tool.compare_reports(baseline, current)
        by_name = {row["name"]: row for row in report["comparisons"]}

        self.assertEqual(by_name["warn boundary"]["verdict"], "warn")
        self.assertEqual(by_name["fail boundary"]["verdict"], "fail")

    def test_scientific_notation_and_large_values_compare_deterministically(self):
        baseline = {"schema_version": 1, "benchmarks": [{"name": "huge", "mean": 1.0e12, "unit": "ns"}]}
        current = {"schema_version": 1, "benchmarks": [{"name": "huge", "mean": 1.06e12, "unit": "ns"}]}

        report = diff_tool.compare_reports(baseline, current)

        self.assertEqual(report["comparisons"][0]["delta"], 60000000000.0)
        self.assertEqual(report["comparisons"][0]["verdict"], "warn")

    def test_change_sort_places_unbounded_regressions_first(self):
        report = diff_tool.compare_reports(load_fixture("baseline.json"), load_fixture("current.json"), sort="change")

        self.assertEqual(report["comparisons"][0]["name"], "zeta zero")

    def test_cli_writes_markdown_and_json_without_failing_by_default(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = pathlib.Path(tmpdir)
            markdown = tmp / "report.md"
            output_json = tmp / "diff.json"

            result = diff_tool.main(
                [
                    "--baseline",
                    str(FIXTURES / "baseline.json"),
                    "--current",
                    str(FIXTURES / "current.json"),
                    "--output-md",
                    str(markdown),
                    "--output-json",
                    str(output_json),
                    "--sort",
                    "verdict",
                ]
            )

            self.assertEqual(result, 0)
            self.assertTrue(markdown.exists())
            self.assertTrue(output_json.exists())
            self.assertIn("# Benchmark Drift Report", markdown.read_text(encoding="utf-8"))
            self.assertEqual(json.loads(output_json.read_text(encoding="utf-8"))["summary"]["fail"], 3)

    def test_cli_fail_on_regression_returns_nonzero_only_when_requested(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = pathlib.Path(tmpdir)
            result = diff_tool.main(
                [
                    "--baseline",
                    str(FIXTURES / "baseline.json"),
                    "--current",
                    str(FIXTURES / "current.json"),
                    "--output-md",
                    str(tmp / "report.md"),
                    "--fail-on-regression",
                ]
            )

            self.assertEqual(result, 2)

    def test_fail_on_regression_also_fails_on_error_or_missing(self):
        report_with_error = {"schema_version": 1, "summary": {"pass": 0, "warn": 0, "fail": 0, "new": 0, "missing": 0, "error": 1}}
        report_with_missing = {"schema_version": 1, "summary": {"pass": 0, "warn": 0, "fail": 0, "new": 0, "missing": 1, "error": 0}}
        report_with_new = {"schema_version": 1, "summary": {"pass": 0, "warn": 0, "fail": 0, "new": 1, "missing": 0, "error": 0}}

        self.assertEqual(diff_tool.exit_code(report_with_error, fail_on_regression=True), 2)
        self.assertEqual(diff_tool.exit_code(report_with_missing, fail_on_regression=True), 2)
        self.assertEqual(diff_tool.exit_code(report_with_new, fail_on_regression=True), 0)

    def test_report_only_mode_does_not_fail_cli_by_default(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            result = diff_tool.main(
                [
                    "--baseline",
                    str(FIXTURES / "baseline.json"),
                    "--current",
                    str(FIXTURES / "current.json"),
                    "--output-md",
                    str(pathlib.Path(tmpdir) / "report.md"),
                ]
            )

            self.assertEqual(result, 0)

    def test_cli_reports_malformed_json(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            stderr = io.StringIO()

            with contextlib.redirect_stderr(stderr):
                result = diff_tool.main(
                    [
                        "--baseline",
                        str(FIXTURES / "malformed.json"),
                        "--current",
                        str(FIXTURES / "current.json"),
                        "--output-md",
                        str(pathlib.Path(tmpdir) / "report.md"),
                    ]
                )

            self.assertEqual(result, 1)
            self.assertIn("is not valid JSON", stderr.getvalue())

    def test_deterministic_markdown_output(self):
        report = diff_tool.compare_reports(
            load_fixture("baseline.json"),
            load_fixture("current.json"),
            sort="verdict",
        )

        markdown_a = diff_tool.markdown_report(report, warn_threshold_percent=5.0, fail_threshold_percent=10.0)
        markdown_b = diff_tool.markdown_report(report, warn_threshold_percent=5.0, fail_threshold_percent=10.0)

        self.assertEqual(markdown_a, markdown_b)
        self.assertIn("| gamma fail | 100 | 112 | ns | 12 | +12.00% | fail |", markdown_a)

    def test_markdown_sanitizes_table_cells(self):
        report = {
            "schema_version": 1,
            "summary": {"pass": 1, "warn": 0, "fail": 0, "new": 0, "missing": 0, "error": 0},
            "comparisons": [
                {
                    "name": "line\nwith|pipe\tand\\slash",
                    "baseline": 1.0,
                    "current": 1.0,
                    "unit": "ns",
                    "delta": 0.0,
                    "delta_percent": 0.0,
                    "verdict": "pass",
                }
            ],
        }

        markdown = diff_tool.markdown_report(report, warn_threshold_percent=5.0, fail_threshold_percent=10.0)

        self.assertIn("line with\\|pipe and\\\\slash", markdown)

    def test_markdown_keeps_unicode_names_readable(self):
        report = {
            "schema_version": 1,
            "summary": {"pass": 1, "warn": 0, "fail": 0, "new": 0, "missing": 0, "error": 0},
            "comparisons": [
                {
                    "name": "유니코드 benchmark",
                    "baseline": 1.0,
                    "current": 1.0,
                    "unit": "ns",
                    "delta": 0.0,
                    "delta_percent": 0.0,
                    "verdict": "pass",
                }
            ],
        }

        markdown = diff_tool.markdown_report(report, warn_threshold_percent=5.0, fail_threshold_percent=10.0)

        self.assertIn("유니코드 benchmark", markdown)


if __name__ == "__main__":
    unittest.main()
