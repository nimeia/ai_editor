#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from pathlib import Path


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".windows_signoff")
    combined = root / "windows_signoff_summary.json"
    if not combined.exists():
      print(f"missing sign-off summary: {combined}", file=sys.stderr)
      return 2
    data = load_json(combined)
    ok = bool(data.get("ok"))
    validation_ok = bool(data.get("validation_ok"))
    rerun_status = str(data.get("rerun_status", ""))
    print(json.dumps({
        "summary": str(combined),
        "ok": ok,
        "validation_ok": validation_ok,
        "rerun_status": rerun_status,
    }, ensure_ascii=False, indent=2))
    return 0 if ok and validation_ok and rerun_status == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
