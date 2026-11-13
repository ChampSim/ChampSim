Benchmark reporting
===================

ChampSim maintainers need benchmark results that can be inspected after a CI run
and compared across revisions. The benchmark reporting tools added for issue
#590 convert Catch2 benchmark XML into stable JSON and Markdown artifacts. They
do not change simulator behavior or benchmark definitions.

Producing a local report
------------------------

Build the Catch2 test binary before running the benchmark command:

.. code-block:: sh

   make test/bin/000-test-main

Run Catch2 with the XML reporter:

.. code-block:: sh

   test/bin/000-test-main \
     --order rand \
     --warn NoAssertions \
     --invisibles \
     --reporter xml::out=catch2-benchmarks.xml

Normalize the XML into JSON and an optional Markdown summary:

.. code-block:: sh

   python3 tools/bench/normalize_catch2_benchmarks.py \
     --input catch2-benchmarks.xml \
     --output benchmark-normalized.json \
     --markdown benchmark-report.md \
     --command 'test/bin/000-test-main --order rand --warn NoAssertions --invisibles --reporter xml::out=catch2-benchmarks.xml'

The normalized JSON has schema version ``1`` and records each benchmark's name,
mean runtime, unit, optional confidence interval fields, sample count, iteration
count, and selected original Catch2 fields. Benchmark rows are sorted by name so
artifact diffs are deterministic.

Comparing two runs
------------------

Compare a baseline report against the current report with:

.. code-block:: sh

   python3 tools/bench/compare_benchmark_results.py \
     --baseline baseline.json \
     --current benchmark-normalized.json \
     --output-md benchmark-diff.md \
     --output-json benchmark-diff.json

By default, comparison is report-only and exits successfully after writing the
artifacts. Use ``--fail-on-regression`` only after maintainers have selected
thresholds that are stable enough for CI gating.

The default thresholds mark at least a 5% runtime increase as ``warn`` and at
least a 10% runtime increase as ``fail``. ``--min-absolute-change`` can suppress
small timing changes that are not meaningful for a noisy benchmark.

Reading the Markdown output
---------------------------

The normalizer Markdown report summarizes each benchmark:

.. code-block:: md

   | Benchmark | Mean | Unit | Low | High | Stddev | Samples | Iterations |
   | --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
   | Finding the value of a folded_shift_register | 2.5 | ns | 2 | 3 | 0.25 | 100 | 8 |

The comparison report starts with verdict counts, then lists the largest warned
or failed regressions, followed by a full details table:

.. code-block:: md

   | Verdict | Count |
   | --- | ---: |
   | fail | 3 |
   | warn | 1 |
   | pass | 2 |

   | Benchmark | Baseline | Current | Unit | Delta | Delta % | Verdict |
   | --- | ---: | ---: | --- | ---: | ---: | --- |
   | gamma fail | 100 | 112 | ns | 12 | +12.00% | fail |

CI artifacts
------------

The manual ``Benchmark Report`` workflow builds the Catch2 test binary, runs the
benchmarks as XML, normalizes the output, and uploads artifacts. The baseline
input is optional.

Uploaded artifacts:

* ``benchmark-normalized-json``: normalized JSON for the current run.
* ``benchmark-report-md``: Markdown summary for the current run.
* ``benchmark-diff-json``: machine-readable comparison output when a baseline
  is supplied.
* ``benchmark-diff-md``: Markdown comparison report when a baseline is supplied.

Deferred follow-ups
-------------------

This first workflow intentionally avoids PR comments, write permissions, and
history publishing. Those features need maintainers to choose a baseline policy,
noise thresholds, and a comment/history update strategy. Artifact-only reporting
keeps the first implementation reviewable and safe for untrusted pull request
code.

Current limitations
-------------------

* The workflow is manual and Linux-only.
* The comparison step needs a normalized baseline JSON supplied by path.
* Benchmark timings can be noisy; thresholds should be tuned with real CI data.
* Only Catch2 XML benchmark output is normalized.
* Duplicate benchmark names and malformed benchmark fields fail normalization.
