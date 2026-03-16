#!/usr/bin/env python3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'sdk' / 'python'))
from bridge_sdk import BridgeClient


def main() -> int:
    cli, workspace = sys.argv[1], sys.argv[2]
    client = BridgeClient(cli, workspace, session_id='sdk-py')
    client.session_begin()
    client.markdown_upsert_section('docs/guide.md', 'SDK', 'python section\n')
    client.json_upsert_key('cfg/settings.json', 'sdk.python', 'true')
    preview = client.session_preview()
    assert preview['previewed_file_count'] == 2
    client.session_commit()
    guide = Path(workspace, 'docs', 'guide.md').read_text(encoding='utf-8')
    cfg = Path(workspace, 'cfg', 'settings.json').read_text(encoding='utf-8')
    assert 'python section' in guide
    assert '"python": true' in cfg
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
