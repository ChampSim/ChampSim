# Codex Notes

Use this repository guidance for future ChampSim automation work.

## Project Rules

- Keep PRs small and reviewable.
- Do not change simulator behavior unless explicitly requested.
- Prefer artifact generation and Markdown reports before PR comments.
- Use Python standard library for tooling unless existing dependencies justify otherwise.
- Always add fixture-based `unittest` coverage for parsing or reporting tools.
- Never claim verification passed unless you actually ran it and saw exit code 0.

## GitHub Actions Safety

- Do not use `pull_request_target` for workflows that run untrusted PR code.
- Do not require secrets for PR workflows.
- Do not request write permissions for artifact-only workflows.
- Prefer `pull_request` with read-only permissions and uploaded artifacts for first-pass CI automation.

## Known Commands

Setup and build paths found in repository workflows:

```bash
vcpkg/bootstrap-vcpkg.sh
vcpkg/vcpkg install
./config.sh
make
make test/bin/000-test-main
```

Test commands found in the Makefile, docs, and workflows:

```bash
make test
make pytest
PYTHONPATH=$PYTHONPATH:$(pwd) python3 -m unittest discover -v --start-directory='test/python'
test/bin/000-test-main --order rand --warn NoAssertions --invisibles
python3 -m json.tool test.json
```

Useful Catch2 discovery commands after building `test/bin/000-test-main`:

```bash
test/bin/000-test-main --list-tests
test/bin/000-test-main --list-reporters
```

## Verification Checklist

- Confirm the intended base branch; workflows commonly target `master` and `develop`.
- Run only the relevant build/test commands for the change.
- For parser/reporting tools, test with committed fixtures and malformed input.
- For CI-only changes, validate YAML behavior through a real workflow run before claiming it works in CI.
- Record any command that could not be run and the exact reason.
