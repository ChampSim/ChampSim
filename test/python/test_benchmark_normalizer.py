import contextlib
import importlib.util
import io
import json
import pathlib
import shutil
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools" / "bench" / "normalize_catch2_benchmarks.py"
FIXTURES = ROOT / "test" / "python" / "fixtures" / "catch2_benchmarks"


def load_tool():
    spec = importlib.util.spec_from_file_location("normalize_catch2_benchmarks", TOOL)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


normalizer = load_tool()


class Catch2BenchmarkNormalizerTests(unittest.TestCase):
    def test_normal_valid_output(self):
        report = normalizer.normalize_catch2_xml(FIXTURES / "catch2_benchmarks.xml", command="test command", commit="abc123")

        self.assertEqual(report["schema_version"], 1)
        self.assertEqual(report["source"], {"tool": "catch2", "command": "test command", "commit": "abc123"})
        self.assertEqual(len(report["benchmarks"]), 3)

        folded = report["benchmarks"][1]
        self.assertEqual(folded["name"], "Pushing into a folded_shift_register")
        self.assertEqual(folded["mean"], 11.5)
        self.assertEqual(folded["unit"], "ns")
        self.assertEqual(folded["low"], 10.0)
        self.assertEqual(folded["high"], 13.0)
        self.assertEqual(folded["stddev"], 1.25)
        self.assertEqual(folded["samples"], 100)
        self.assertEqual(folded["iterations"], 4)
        self.assertEqual(folded["raw"]["mean_ci"], 0.95)

    def test_multiple_benchmarks_have_deterministic_name_order(self):
        report = normalizer.normalize_catch2_xml(FIXTURES / "catch2_benchmarks.xml")
        self.assertEqual(
            [benchmark["name"] for benchmark in report["benchmarks"]],
            [
                "Finding the value of a folded_shift_register",
                "Pushing into a folded_shift_register",
                "name with spaces/slashes_and_underscores",
            ],
        )

    def test_names_can_contain_spaces_slashes_and_underscores(self):
        report = normalizer.normalize_catch2_xml(FIXTURES / "catch2_benchmarks.xml")
        self.assertIn("name with spaces/slashes_and_underscores", [b["name"] for b in report["benchmarks"]])

    def test_units_ns_us_ms_and_s_are_preserved_when_present(self):
        report = normalizer.normalize_catch2_xml(FIXTURES / "catch2_units.xml")
        self.assertEqual({benchmark["unit"] for benchmark in report["benchmarks"]}, {"ns", "us", "ms", "s"})

    def test_missing_optional_fields_become_null(self):
        report = normalizer.normalize_catch2_xml(FIXTURES / "catch2_missing_optional.xml")
        benchmark = report["benchmarks"][0]

        self.assertEqual(benchmark["name"], "minimal benchmark")
        self.assertEqual(benchmark["mean"], 42.0)
        self.assertIsNone(benchmark["low"])
        self.assertIsNone(benchmark["high"])
        self.assertIsNone(benchmark["stddev"])
        self.assertIsNone(benchmark["samples"])
        self.assertIsNone(benchmark["iterations"])

    def test_xml_namespaces_are_supported(self):
        report = normalizer.normalize_catch2_xml(FIXTURES / "catch2_namespace.xml")
        benchmark = report["benchmarks"][0]

        self.assertEqual(benchmark["name"], "namespaced benchmark")
        self.assertEqual(benchmark["mean"], 7.0)
        self.assertEqual(benchmark["samples"], 2)
        self.assertEqual(benchmark["iterations"], 3)

    def test_input_without_benchmarks_is_rejected(self):
        with self.assertRaisesRegex(normalizer.BenchmarkParseError, r"does not contain Catch2 BenchmarkResults elements"):
            normalizer.normalize_catch2_xml(FIXTURES / "catch2_no_benchmarks.xml")

    def test_malformed_input_raises(self):
        with self.assertRaisesRegex(
            normalizer.BenchmarkParseError,
            r"catch2_malformed\.xml: benchmark 'missing mean' is missing mean results",
        ):
            normalizer.normalize_catch2_xml(FIXTURES / "catch2_malformed.xml")

    def test_invalid_xml_reports_path(self):
        with self.assertRaisesRegex(normalizer.BenchmarkParseError, r"catch2_invalid\.xml is not valid XML"):
            normalizer.normalize_catch2_xml(FIXTURES / "catch2_invalid.xml")

    def test_unsupported_unit_reports_benchmark_name(self):
        with self.assertRaisesRegex(
            normalizer.BenchmarkParseError,
            r"benchmark 'unsupported unit' has unsupported benchmark unit 'fortnights'",
        ):
            normalizer.normalize_catch2_xml(FIXTURES / "catch2_unsupported_unit.xml")

    def test_failed_benchmark_reports_catch2_message(self):
        with self.assertRaisesRegex(
            normalizer.BenchmarkParseError,
            r"benchmark 'failed benchmark' failed in Catch2 report: benchmark setup failed",
        ):
            normalizer.normalize_catch2_xml(FIXTURES / "catch2_failed_benchmark.xml")

    def test_non_numeric_optional_field_reports_context(self):
        with self.assertRaisesRegex(
            normalizer.BenchmarkParseError,
            r"benchmark 'bad optional' has non-integer 'samples': 'one'",
        ):
            normalizer.normalize_catch2_xml(FIXTURES / "catch2_non_numeric_optional.xml")

    def test_duplicate_benchmark_names_are_rejected(self):
        with self.assertRaisesRegex(normalizer.BenchmarkParseError, r"duplicate benchmark name 'duplicate benchmark'"):
            normalizer.normalize_catch2_xml(FIXTURES / "catch2_duplicate_names.xml")

    def test_non_finite_metric_is_rejected(self):
        with self.assertRaisesRegex(normalizer.BenchmarkParseError, r"benchmark 'nan mean' mean has invalid 'value': 'NaN'"):
            normalizer.normalize_catch2_xml(FIXTURES / "catch2_invalid_number.xml")

    def test_negative_metric_is_rejected(self):
        with self.assertRaisesRegex(normalizer.BenchmarkParseError, r"benchmark 'negative mean' mean has invalid 'value': '-1'"):
            normalizer.normalize_catch2_xml(FIXTURES / "catch2_negative_value.xml")

    def test_cli_writes_deterministic_json_and_markdown(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = pathlib.Path(tmpdir)
            output_a = tmp / "a.json"
            output_b = tmp / "b.json"
            markdown = tmp / "summary.md"

            args = [
                "--input",
                str(FIXTURES / "catch2_benchmarks.xml"),
                "--output",
                str(output_a),
                "--markdown",
                str(markdown),
                "--command",
                "test/bin/000-test-main --reporter XML::out=catch2-benchmarks.xml",
            ]
            self.assertEqual(normalizer.main(args), 0)

            args[args.index(str(output_a))] = str(output_b)
            self.assertEqual(normalizer.main(args), 0)

            self.assertEqual(output_a.read_text(encoding="utf-8"), output_b.read_text(encoding="utf-8"))
            parsed = json.loads(output_a.read_text(encoding="utf-8"))
            self.assertEqual(parsed["benchmarks"][0]["name"], "Finding the value of a folded_shift_register")
            self.assertIn("| Benchmark | Mean | Unit |", markdown.read_text(encoding="utf-8"))

    def test_cli_handles_paths_with_spaces(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = pathlib.Path(tmpdir)
            input_dir = tmp / "input dir"
            output_dir = tmp / "output dir"
            input_dir.mkdir()
            fixture = input_dir / "catch2 benchmarks.xml"
            shutil.copyfile(FIXTURES / "catch2_benchmarks.xml", fixture)

            output = output_dir / "normalized output.json"
            markdown = output_dir / "summary output.md"

            result = normalizer.main(["--input", str(fixture), "--output", str(output), "--markdown", str(markdown)])
            self.assertEqual(result, 0)
            self.assertEqual(len(json.loads(output.read_text(encoding="utf-8"))["benchmarks"]), 3)
            self.assertIn("Catch2 Benchmark Summary", markdown.read_text(encoding="utf-8"))

    def test_cli_returns_nonzero_on_malformed_input(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            output = pathlib.Path(tmpdir) / "out.json"
            stderr = io.StringIO()

            with contextlib.redirect_stderr(stderr):
                result = normalizer.main(["--input", str(FIXTURES / "catch2_malformed.xml"), "--output", str(output)])

            self.assertEqual(result, 1)
            self.assertFalse(output.exists())
            self.assertIn("benchmark 'missing mean' is missing mean results", stderr.getvalue())

    def test_markdown_sanitizes_table_cells(self):
        report = {
            "benchmarks": [
                {
                    "name": "line\nwith|pipe\tand\\slash",
                    "mean": 1.0,
                    "unit": "ns",
                    "low": None,
                    "high": None,
                    "stddev": None,
                    "samples": None,
                    "iterations": None,
                }
            ]
        }

        markdown = normalizer.markdown_summary(report)

        self.assertIn("line with\\|pipe and\\\\slash", markdown)


if __name__ == "__main__":
    unittest.main()
