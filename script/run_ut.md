# run_ut.sh Usage Guide

## Mode Comparison

| Feature | Default mode | `--fast` mode |
|---|---|---|
| Build directory cleanup | Full wipe before build | Skipped (incremental) |
| cmake reconfiguration | Always | Only on first run or fingerprint mismatch |
| Coverage report (lcov/genhtml) | Generated | Skipped |
| Typical use case | CI / pre-commit | Local development iteration |

## Usage

```bash
# Default: full clean build + coverage report
bash script/run_ut.sh

# Default with test filter
bash script/run_ut.sh SmemBmTest

# Fast: incremental build, no coverage
bash script/run_ut.sh --fast

# Fast with test filter
bash script/run_ut.sh --fast SmemBmTest
```

## `--fast` Fingerprint Mechanism

The `--fast` mode writes a fingerprint string (`ASAN-UT-OPEN_ABI`) to
`build/.build_fingerprint` after the first successful cmake configuration.
On subsequent runs it checks:

1. **Build directory missing** or **fingerprint file missing** → full build
2. **Fingerprint mismatch** (build config changed externally) → full rebuild
3. **Fingerprint matches** → incremental build (skip cmake, skip rm)

The three cases print:
```
========= first build, full build ============
========= build config changed, full rebuild ============
========= incremental build ============
```

## Recommended Workflow

- During active development, use `bash script/run_ut.sh --fast` for fast
  iteration (recompiles only changed translation units).
- Before committing or pushing, run `bash script/run_ut.sh` to get the full
  clean build and coverage report required by CI.
