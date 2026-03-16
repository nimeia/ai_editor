# Benchmark Suite

This directory contains the controlled-editing benchmark suite.

## Layout
- `scenarios/*.json`: declarative benchmark scenarios
- `runner.py`: execute one scenario and emit a step-level JSON report
- `suite.py`: execute the full scenario directory, aggregate metrics, and enforce thresholds
- `thresholds.json`: suite/scenario gate configuration
- `reports/`: optional output directory for generated reports

## CI gate

```bash
bash ./scripts/run_benchmarks.sh build-benchmark .benchmark_reports .benchmark_run
```

The script starts a daemon, prepares a benchmark workspace, runs the suite, and writes JSON and Markdown reports. The CI job fails when the report violates `benchmark/thresholds.json`.
