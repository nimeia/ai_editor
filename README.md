# ai_bridge

A cross-platform Agent Text Workspace Bridge.

## Current status

The repository is now at a **V1-ready baseline**:

- M1: transport / handshake baseline completed
- M2: workspace + path policy completed
- M3: file service completed (`fs.list`, `fs.stat`, `fs.read`, `fs.read_range`)
- M4: search service completed (`search.text`, `search.regex`)
- M5: patch service completed (`patch.preview`, `patch.apply`, `patch.rollback`, `history.list`)
- M6: runtime / audit logging baseline completed
- Windows native runtime / transport baseline completed with CI, smoke script, and PowerShell native integration coverage
- Timeout / cancel baseline completed across search, streaming file reads, and streaming patch preview
- Streaming baseline completed for search, file reads, and patch preview
- Patch preview lifecycle + conflict diagnostics + rollback metadata completed

P6 validation assets are now in place. Linux/POSIX validation has been rerun in this phase; Windows still needs one native rerun before final release sign-off.

## Implemented capabilities

### Core methods

- `daemon.ping`
- `workspace.info`
- `workspace.open`
- `workspace.resolve_path`
- `fs.list`
- `fs.stat`
- `fs.read`
- `fs.read_range`
- `search.text`
- `search.regex`
- `request.cancel`
- `patch.preview`
- `patch.apply`
- `patch.rollback`
- `history.list`

### Runtime / platform

- POSIX runtime + Unix domain socket transport
- Windows runtime + Named Pipe transport
- Instance identity derived from `user + workspace + profile + policy`
- Per-instance runtime directory / endpoint / lock
- `workspace.info` / `workspace.open` diagnostics fields:
  - `runtime_dir`
  - `platform`
  - `transport`

### Workspace safety

- path normalization
- workspace containment checks
- path policy states:
  - `normal`
  - `skip_by_default`
  - `deny`

### File metadata

- binary/text detection
- encoding metadata
- BOM detection
- EOL detection
- line count
- size / mtime

### Stream / timeout / cancel

- search streaming: `search-text --stream`, `search-regex --stream`
- file streaming: `read --stream`, `read-range --stream`
- patch streaming: `patch-preview --stream`
- stream final summaries consistently include:
  - `stream_event`
  - `chunk_count`
  - `cancelled`
  - `timed_out`
- `--timeout-ms` is forwarded to server-side processing budgets for search / stream reads / patch preview streams
- CLI keeps a small extra transport grace window so structured timeout responses can still be returned
- `request.cancel` is enforced for search, streaming file reads, and streaming patch preview

### Patch lifecycle

- `patch.preview` returns `preview_id`
- `patch.apply --preview-id <id>` supports preview-then-commit flow
- patch previews expose:
  - `preview_created_at`
  - `preview_expires_at`
- preview lifecycle errors are distinguished as:
  - `PREVIEW_CONSUMED`
  - `PREVIEW_EXPIRED`
  - `PREVIEW_EVICTED`
  - `PREVIEW_INVALID`
- patch conflict errors remain `PATCH_CONFLICT`, but now include a more specific reason such as:
  - `mtime_changed`
  - `hash_changed`
  - `mtime_and_hash_changed`
- `patch.rollback` returns restored file hash / mtime metadata

## Project layout

- `apps/bridge_daemon`: local daemon
- `apps/bridge_cli`: command line client
- `libs/bridge_core`: protocol, workspace, path policy, file/search/patch/logging/error-code services
- `libs/bridge_platform`: runtime dir, lock, endpoint helpers (POSIX/Windows split)
- `libs/bridge_transport`: local IPC transport (Unix socket / Named Pipe)
- `tests`: unit and integration tests
- `docs`: architecture, protocol, plan, risks, run/test guide

## Build

### POSIX build (Linux/macOS-style)

```bash
cmake -S . -B build
cmake --build build -j1
ctest --test-dir build --output-on-failure
```

### Windows build (PowerShell)

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Manual run

### POSIX example

Terminal 1:

```bash
./build/apps/bridge_daemon/bridge_daemon --workspace "$PWD"
```

Terminal 2:

```bash
./build/apps/bridge_cli/bridge_cli ping --workspace "$PWD"
./build/apps/bridge_cli/bridge_cli info --workspace "$PWD"
./build/apps/bridge_cli/bridge_cli open --workspace "$PWD"
./build/apps/bridge_cli/bridge_cli resolve --workspace "$PWD" --path docs/04-V1-协议草案.md
./build/apps/bridge_cli/bridge_cli list --workspace "$PWD"
./build/apps/bridge_cli/bridge_cli stat --workspace "$PWD" --path README.md
./build/apps/bridge_cli/bridge_cli read --workspace "$PWD" --path README.md
./build/apps/bridge_cli/bridge_cli read-range --workspace "$PWD" --path README.md --start 1 --end 10
./build/apps/bridge_cli/bridge_cli search-text --workspace "$PWD" --query bridge --exts .md,.cpp
./build/apps/bridge_cli/bridge_cli search-regex --workspace "$PWD" --pattern main --exts .cpp
./build/apps/bridge_cli/bridge_cli patch-preview --workspace "$PWD" --path README.md --new-content-file /tmp/new.txt --json
./build/apps/bridge_cli/bridge_cli patch-apply --workspace "$PWD" --path README.md --preview-id <preview_id>
./build/apps/bridge_cli/bridge_cli history --workspace "$PWD" --path README.md
./build/apps/bridge_cli/bridge_cli cancel --workspace "$PWD" --target-request-id req-123 --request-id cancel-req
```

### Windows native smoke

Windows native CTest now runs `tests/functional_windows_native.ps1` (the legacy `tests/integration_windows_native.ps1` remains as a compatibility wrapper).
You can also run the repository smoke manually against an existing build directory:

```powershell
pwsh ./scripts/windows_smoke.ps1 -BuildDir ./build -Config Release
```

The smoke and native integration cover:

- daemon start / stop
- ping / info / open
- unicode + space workspace / file paths
- backslash-path normalization
- list / stat / read / search-text
- read streaming
- patch-preview / patch-apply / history
- runtime log / audit log verification
- direct daemon stdout/stderr dump on failure

## Release engineering baseline

P5 is now in place:

- install layout via `cmake --install`
- archive packaging via `cpack`
- version-aware artifact naming
- release packaging scripts for POSIX and Windows
- `--version` support in both `bridge_cli` and `bridge_daemon`
- package checksum file generation (`SHA256SUMS.txt`)

### Install layout

```text
<prefix>/
  bin/
    bridge_daemon(.exe)
    bridge_cli(.exe)
  share/ai_bridge/
    README.md
    docs/
    scripts/
```

Installed package docs use ASCII filenames in the archive/install tree for portability, while the repository keeps the original source filenames.

### Package commands

POSIX:

```bash
./scripts/package_release.sh --build-dir build --out-dir dist --generator TGZ --run-tests --jobs 1
```

Windows:

```powershell
pwsh ./scripts/package_release.ps1 -BuildDir build -Config Release -OutDir dist -Generator ZIP -RunTests -Jobs 1
```

### End-to-end validation scripts

- POSIX: `./scripts/validate_v1.sh --build-dir build --jobs 1`
- Windows: `pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1`
  - 摘要输出：`.p6_validation_summary/windows_validation_summary.json`
  - 可读报告：`.p6_validation_summary/windows_validation_summary.md`

See `docs/09-V1-验证报告.md` for the current validation status and `docs/10-V1-最终发布检查清单.md` for final sign-off.

### GitHub Actions

The repository now includes two workflow entry points:

- `.github/workflows/ci.yml`
  - push / pull_request / workflow_dispatch
  - validates Linux, macOS, and Windows
  - reuses `scripts/validate_v1.sh` and `scripts/validate_v1.ps1`
  - uploads validation outputs as workflow artifacts
- `.github/workflows/release.yml`
  - runs on tags like `v*` and on manual dispatch
  - rebuilds and validates Linux, macOS, and Windows
  - uploads packaged archives and publishes them to a GitHub Release

If you only need CI checks, use `ci.yml`. If you want signed-off release artifacts attached to a GitHub Release, use `release.yml`.

See `docs/08-V1-发布与部署指南.md` for the detailed workflow behavior and release expectations.

### Version commands

```bash
./build/apps/bridge_cli/bridge_cli --version
./build/apps/bridge_daemon/bridge_daemon --version
```

For more release details, see `docs/08-V1-发布与部署指南.md`.

## Logs and state

### Runtime / workspace state

- POSIX runtime log: `${XDG_RUNTIME_DIR:-/tmp/ai_bridge_runtime}/<uid>/runtime.log`
- Windows runtime log: under the per-instance `runtime_dir` returned by `workspace.info` / `workspace.open`
- Audit log: `<workspace>/.bridge/audit.log`
- Patch history: `<workspace>/.bridge/history.log`
- Patch previews: `<workspace>/.bridge/previews/`
- Patch backups: `<workspace>/.bridge/backups/`

### Rotation / cleanup environment overrides

- `AI_BRIDGE_LOG_ROTATE_BYTES`
- `AI_BRIDGE_LOG_ROTATE_KEEP`
- `AI_BRIDGE_HISTORY_ROTATE_BYTES`
- `AI_BRIDGE_HISTORY_ROTATE_KEEP`
- `AI_BRIDGE_BACKUP_KEEP`
- `AI_BRIDGE_PREVIEW_KEEP`
- `AI_BRIDGE_PREVIEW_TTL_MS`
- `AI_BRIDGE_PREVIEW_STATUS_KEEP`

## Error codes

Mainline request handling returns normalized error codes such as:

- `PATH_OUTSIDE_WORKSPACE`
- `DENIED_BY_POLICY`
- `FILE_NOT_FOUND`
- `INVALID_PARAMS`
- `BINARY_FILE`
- `ACCESS_DENIED`
- `SEARCH_TIMEOUT`
- `REQUEST_TIMEOUT`
- `REQUEST_CANCELLED`
- `PATCH_CONFLICT`
- `ROLLBACK_NOT_FOUND`
- `PREVIEW_CONSUMED`
- `PREVIEW_EXPIRED`
- `PREVIEW_EVICTED`
- `PREVIEW_INVALID`
- `UNSUPPORTED_METHOD`
- `INTERNAL_ERROR`

## Notes

- Default CLI output is human-friendly.
- `--json` returns raw JSON for scripts and tests.
- In `--stream --json` mode, CLI prints one JSON frame per line.
- Non-stream commands remain backward compatible and return a single response.
- Current V1 does not implement remote access, semantic retrieval, or multi-file merge strategies.

For a more detailed build / run / test walkthrough, see `docs/07-V1-构建运行与测试指南.md`.
