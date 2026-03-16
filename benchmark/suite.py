#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
from typing import Any

from runner import load_json, run_scenario


def scenario_thresholds(thresholds: dict[str, Any], name: str) -> dict[str, Any]:
    merged: dict[str, Any] = {}
    merged.update(thresholds.get('default_scenario', {}))
    merged.update(thresholds.get('scenarios', {}).get(name, {}))
    return merged


def evaluate(summary: dict[str, Any], thresholds: dict[str, Any]) -> list[str]:
    violations: list[str] = []
    suite = thresholds.get('suite', {})
    if 'min_pass_rate' in suite and summary['pass_rate'] < float(suite['min_pass_rate']):
        violations.append(f"suite pass_rate {summary['pass_rate']:.3f} < {float(suite['min_pass_rate']):.3f}")
    if 'max_failed_scenarios' in suite and summary['failed_scenarios'] > int(suite['max_failed_scenarios']):
        violations.append(f"suite failed_scenarios {summary['failed_scenarios']} > {int(suite['max_failed_scenarios'])}")
    if 'max_total_duration_ms' in suite and summary['total_duration_ms'] > int(suite['max_total_duration_ms']):
        violations.append(f"suite total_duration_ms {summary['total_duration_ms']} > {int(suite['max_total_duration_ms'])}")
    for scenario in summary['scenarios']:
        limits = scenario_thresholds(thresholds, scenario['name'])
        if limits.get('required', True) and not scenario['ok']:
            violations.append(f"scenario {scenario['name']} failed")
        if 'min_step_pass_rate' in limits and scenario['step_pass_rate'] < float(limits['min_step_pass_rate']):
            violations.append(f"scenario {scenario['name']} step_pass_rate {scenario['step_pass_rate']:.3f} < {float(limits['min_step_pass_rate']):.3f}")
        if 'max_duration_ms' in limits and scenario['duration_ms'] > int(limits['max_duration_ms']):
            violations.append(f"scenario {scenario['name']} duration_ms {scenario['duration_ms']} > {int(limits['max_duration_ms'])}")
    return violations


def markdown(summary: dict[str, Any]) -> str:
    lines = [
        '# Benchmark Report',
        '',
        f"- Total scenarios: {summary['total_scenarios']}",
        f"- Passed scenarios: {summary['passed_scenarios']}",
        f"- Failed scenarios: {summary['failed_scenarios']}",
        f"- Pass rate: {summary['pass_rate']:.3f}",
        f"- Total duration (ms): {summary['total_duration_ms']}",
        '',
        '| Scenario | OK | Steps | Failed Steps | Step Pass Rate | Duration (ms) |',
        '|---|---:|---:|---:|---:|---:|',
    ]
    for scenario in summary['scenarios']:
        lines.append(
            f"| {scenario['name']} | {'yes' if scenario['ok'] else 'no'} | {scenario['step_count']} | {scenario['failed_step_count']} | {scenario['step_pass_rate']:.3f} | {scenario['duration_ms']} |"
        )
    lines.extend(['', '## Threshold violations', ''])
    if summary['violations']:
        lines.extend(f'- {violation}' for violation in summary['violations'])
    else:
        lines.append('- none')
    return '\n'.join(lines) + '\n'


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--cli', required=True)
    parser.add_argument('--scenario-dir', required=True)
    parser.add_argument('--vars', required=True)
    parser.add_argument('--thresholds')
    parser.add_argument('--output-json')
    parser.add_argument('--output-md')
    args = parser.parse_args()

    vars_ = load_json(Path(args.vars))
    thresholds = load_json(Path(args.thresholds)) if args.thresholds else {}

    scenarios = []
    passed = 0
    total_duration = 0
    for scenario_path in sorted(Path(args.scenario_dir).glob('*.json')):
        report = run_scenario(args.cli, load_json(scenario_path), vars_)
        scenario = {
            'name': report['name'],
            'ok': report['ok'],
            'step_count': len(report['steps']),
            'failed_step_count': sum(1 for step in report['steps'] if not step['ok']),
            'step_pass_rate': report['step_pass_rate'],
            'duration_ms': report['duration_ms'],
            'report': report,
        }
        scenarios.append(scenario)
        total_duration += scenario['duration_ms']
        if scenario['ok']:
            passed += 1

    summary = {
        'total_scenarios': len(scenarios),
        'passed_scenarios': passed,
        'failed_scenarios': len(scenarios) - passed,
        'pass_rate': 1.0 if not scenarios else passed / len(scenarios),
        'total_duration_ms': total_duration,
        'scenarios': scenarios,
    }
    summary['violations'] = evaluate(summary, thresholds)
    summary['ok'] = not summary['violations'] and summary['failed_scenarios'] == 0

    if args.output_json:
        Path(args.output_json).write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    if args.output_md:
        Path(args.output_md).write_text(markdown(summary), encoding='utf-8')

    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0 if summary['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
