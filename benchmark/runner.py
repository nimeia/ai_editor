#!/usr/bin/env python3
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding='utf-8'))

def expand(items, vars_):
    out = []
    for item in items:
        value = item
        for k, v in vars_.items():
            value = value.replace('${' + k + '}', str(v))
        out.append(value)
    return out

def json_get(payload, path):
    cur = payload
    for part in path.split('.'):
        if isinstance(cur, list):
            cur = cur[int(part)]
        else:
            cur = cur[part]
    return cur

def run_scenario(cli: str, scenario: dict[str, Any], vars_: dict[str, Any]) -> dict[str, Any]:
    report = {'name': scenario.get('name', 'unnamed'), 'steps': []}
    for step in scenario.get('steps', []):
        cmd = [cli] + expand(step['command'], vars_)
        start = time.time()
        proc = subprocess.run(cmd, capture_output=True, text=True)
        elapsed = int((time.time() - start) * 1000)
        stdout = proc.stdout
        stderr = proc.stderr
        contains_ok = all(s in stdout for s in step.get('expect_contains', []))
        json_ok = True
        json_error = ''
        if step.get('expect_json'):
            try:
                parsed = json.loads(stdout)
                for path, expected in step['expect_json'].items():
                    actual = json_get(parsed, path)
                    if str(actual) != str(expected):
                        json_ok = False
                        json_error = f'{path} expected {expected} got {actual}'
                        break
            except Exception as exc:
                json_ok = False
                json_error = str(exc)
        ok = proc.returncode == 0 and contains_ok and json_ok
        report['steps'].append({'command': cmd,'exit_code': proc.returncode,'duration_ms': elapsed,'ok': ok,'stdout': stdout,'stderr': stderr,'expect_contains': step.get('expect_contains', []),'expect_json': step.get('expect_json', {}),'json_error': json_error})
    report['ok'] = all(s['ok'] for s in report['steps'])
    report['step_count'] = len(report['steps'])
    report['failed_step_count'] = sum(1 for s in report['steps'] if not s['ok'])
    report['duration_ms'] = sum(int(s['duration_ms']) for s in report['steps'])
    report['step_pass_rate'] = 1.0 if report['step_count'] == 0 else ((report['step_count'] - report['failed_step_count']) / report['step_count'])
    return report

def main():
    if len(sys.argv) < 4:
        print('usage: runner.py <cli> <scenario.json> <vars.json>', file=sys.stderr)
        return 2
    cli = sys.argv[1]
    scenario = load_json(Path(sys.argv[2]))
    vars_ = load_json(Path(sys.argv[3]))
    report = run_scenario(cli, scenario, vars_)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report['ok'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
