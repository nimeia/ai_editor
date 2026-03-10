#!/usr/bin/env python3
import json
import sys
from typing import Any


def usage() -> int:
    print(
        "usage: json_assert.py <get|eq|contains|truthy|falsey|len_ge|absent> <path> [value]",
        file=sys.stderr,
    )
    return 2


def split_path(path: str) -> list[str]:
    if not path:
      return []
    return [part for part in path.split('.') if part != '']


def resolve(doc: Any, path: str) -> Any:
    cur = doc
    for part in split_path(path):
        if isinstance(cur, list):
            try:
                idx = int(part)
            except ValueError as exc:
                raise KeyError(f"list index expected at '{part}'") from exc
            try:
                cur = cur[idx]
            except IndexError as exc:
                raise KeyError(f"list index out of range: {idx}") from exc
        elif isinstance(cur, dict):
            if part not in cur:
                raise KeyError(f"missing key: {part}")
            cur = cur[part]
        else:
            raise KeyError(f"cannot descend into scalar at '{part}'")
    return cur


def scalar_to_text(value: Any) -> str:
    if value is True:
        return 'true'
    if value is False:
        return 'false'
    if value is None:
        return 'null'
    return str(value)


if len(sys.argv) < 3:
    raise SystemExit(usage())

cmd = sys.argv[1]
path = sys.argv[2]
expected = sys.argv[3] if len(sys.argv) >= 4 else None

try:
    doc = json.load(sys.stdin)
except json.JSONDecodeError as exc:
    print(f"invalid json: {exc}", file=sys.stderr)
    raise SystemExit(1)

if cmd == 'absent':
    try:
        resolve(doc, path)
    except KeyError:
        raise SystemExit(0)
    print(f"expected path to be absent: {path}", file=sys.stderr)
    raise SystemExit(1)

try:
    actual = resolve(doc, path)
except KeyError as exc:
    print(str(exc), file=sys.stderr)
    raise SystemExit(1)

if cmd == 'get':
    if isinstance(actual, (dict, list)):
        print(json.dumps(actual, ensure_ascii=False))
    else:
        print(scalar_to_text(actual))
    raise SystemExit(0)

if cmd == 'eq':
    if expected is None:
        raise SystemExit(usage())
    if scalar_to_text(actual) != expected:
        print(f"expected {path} == {expected!r}, actual={scalar_to_text(actual)!r}", file=sys.stderr)
        raise SystemExit(1)
    raise SystemExit(0)

if cmd == 'contains':
    if expected is None:
        raise SystemExit(usage())
    haystack = actual if isinstance(actual, str) else json.dumps(actual, ensure_ascii=False)
    if expected not in haystack:
        print(f"expected {path} to contain {expected!r}, actual={haystack!r}", file=sys.stderr)
        raise SystemExit(1)
    raise SystemExit(0)

if cmd == 'truthy':
    if not actual:
        print(f"expected {path} to be truthy, actual={actual!r}", file=sys.stderr)
        raise SystemExit(1)
    raise SystemExit(0)

if cmd == 'falsey':
    if actual:
        print(f"expected {path} to be falsey, actual={actual!r}", file=sys.stderr)
        raise SystemExit(1)
    raise SystemExit(0)

if cmd == 'len_ge':
    if expected is None:
        raise SystemExit(usage())
    try:
        minimum = int(expected)
    except ValueError:
        print(f"len_ge expects integer, got {expected!r}", file=sys.stderr)
        raise SystemExit(2)
    if not hasattr(actual, '__len__'):
        print(f"object at {path} has no len(): {actual!r}", file=sys.stderr)
        raise SystemExit(1)
    size = len(actual)
    if size < minimum:
        print(f"expected len({path}) >= {minimum}, actual={size}", file=sys.stderr)
        raise SystemExit(1)
    raise SystemExit(0)

raise SystemExit(usage())
