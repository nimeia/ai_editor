# ai_bridge

A cross-platform Agent Text Workspace Bridge.

## Current status

The repository is now at a **V1-ready baseline**:

- M1: transport / handshake baseline completed
- M2: workspace + path policy completed
- M3: file service completed (`fs.list`, `fs.stat`, `fs.read`, `fs.read_range`, `fs.write`, `fs.mkdir`, `fs.move`, `fs.copy`, `fs.rename`)
- M4: search service completed (`search.text`, `search.regex`)
- M5: patch service completed (`patch.preview`, `patch.apply`, `patch.rollback`, `history.list`)
- M6: runtime / audit logging baseline completed
- Windows native runtime / transport baseline completed with CI, smoke script, and PowerShell native integration coverage
- Timeout / cancel baseline completed across search, streaming file reads, and streaming patch preview
- Streaming baseline completed for search, file reads, and patch preview
- Patch preview lifecycle + conflict diagnostics + rollback metadata completed

P6 validation assets are now in place. Linux/POSIX validation has been rerun in this phase; Windows now has a fuller functional matrix for native smoke, stream/cancel/timeout, CLI contract, logging code consistency, logging success completeness, and release/package closure, but still needs one native rerun before final release sign-off.

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
- `fs.write`
- `fs.mkdir`
- `fs.move`
- `fs.copy`
- `fs.rename`
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

### Text editing baseline

- direct text writes: `fs.write`
- directory creation: `fs.mkdir`
- file/directory move: `fs.move`
- file/directory copy: `fs.copy`
- same-directory rename: `fs.rename`
- `fs.write` creates parent directories by default
- move/copy create parent directories by default
- overwrite control is available for write/move/copy/rename paths where applicable
- patch preview/apply can now target a file that does not yet exist
- rollback for a newly created file restores the original absent state
- patch preview diff output is now narrowed to a contextual hunk instead of a full-file replacement dump

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

## Test matrix

### Shared unit / core tests

- `test_instance`
- `test_path_policy`
- `test_file_service`
- `test_file_stream`
- `test_search_service`
- `test_patch_service`
- `test_platform_transport`
- `test_logging`
- `test_error_codes`
- `test_file_service_edges`
- `test_search_service_edges`
- `test_patch_service_edges`
- `test_bridge_cli_version`
- `test_bridge_daemon_version`

### POSIX integration / functional tests

- integration: `test_integration_ping_info`, `test_integration_file_ops`, `test_integration_search_ops`, `test_integration_patch_ops`, `test_integration_logging_ops`, `test_integration_cancel_ops`, `test_integration_stream_ops`, `test_integration_read_stream_ops`, `test_integration_patch_stream_ops`, `test_integration_timeout_ops`
- functional: `test_functional_workspace_ops`, `test_functional_workspace_edges`, `test_functional_fs_ops`, `test_functional_search_ops`, `test_functional_patch_lifecycle`, `test_functional_stream_cancel_timeout`, `test_functional_cancel_edges`, `test_functional_logging_release`, `test_functional_cli_contract`

### Session / recovery gate

The `session_recovery` CTest label is wired into CI as an independent gate on Linux.

Run locally:

```bash
ctest --test-dir build -N -L session_recovery
ctest --test-dir build --output-on-failure --parallel 1 -L session_recovery
```

Coverage in this gate:
- `test_functional_session_drop_change_contract`
- `test_functional_session_drop_change_edges`
- `test_integration_session_ops`
- `test_integration_recovery_ops`
- `test_integration_structure_ops`
- `test_integration_sdk_ops`
- `test_session_service`

### Windows functional tests

- `test_functional_windows_native`
- `test_functional_windows_stream_cancel_timeout`
- `test_functional_windows_cli_contract`
- `test_functional_windows_logging_codes`
- `test_functional_windows_logging_success`
- `test_functional_windows_release_package`

Windows coverage now closes the loop across these domains:

- native smoke and Unicode / space-path coverage
- stream / cancel / timeout behavior
- CLI usability and normalized error-code contract
- runtime.log / audit.log error-path code consistency
- runtime.log / audit.log success-path field completeness
- install / package / unzip smoke

## Recommended execution order

### POSIX

```bash
ctest --test-dir build --output-on-failure
```

For stepwise triage, run in this order:

1. shared unit/core tests
2. POSIX integration tests
3. POSIX functional tests
4. `./scripts/validate_v1.sh --build-dir build --jobs 1`
5. `./scripts/package_release.sh --build-dir build --out-dir dist --generator TGZ --run-tests --jobs 1`

### Windows

Recommended stepwise order on a native Windows host:

```powershell
ctest --test-dir build -C Release -R "^(test_instance|test_path_policy|test_file_service|test_file_stream|test_search_service|test_patch_service|test_platform_transport|test_logging|test_error_codes|test_file_service_edges|test_search_service_edges|test_patch_service_edges|test_bridge_cli_version|test_bridge_daemon_version)$" --output-on-failure
ctest --test-dir build -C Release -R "^test_functional_windows_native$" --output-on-failure
ctest --test-dir build -C Release -R "^test_functional_windows_stream_cancel_timeout$" --output-on-failure
ctest --test-dir build -C Release -R "^test_functional_windows_cli_contract$" --output-on-failure
ctest --test-dir build -C Release -R "^test_functional_windows_logging_(codes|success)$" --output-on-failure
ctest --test-dir build -C Release -R "^test_functional_windows_release_package$" --output-on-failure
```

For a one-shot rerun:

```powershell
ctest --test-dir build -C Release --output-on-failure
pwsh ./scripts/windows_smoke.ps1 -BuildDir ./build -Config Release
pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1
pwsh ./scripts/package_release.ps1 -BuildDir build -Config Release -OutDir dist -Generator ZIP -RunTests -Jobs 1
```


## Controlled editing additions

This repository now includes:
- structure adapter methods for Markdown / JSON / YAML / HTML on top of the controlled-editing session pipeline
- minimal Python and TypeScript SDK wrappers in `sdk/python` and `sdk/typescript`
- benchmark scenarios for structure roundtrip and multi-file session risk checks

Representative CLI commands:
- `markdown-upsert-section`
- `json-upsert-key`
- `yaml-append-item`
- `html-set-attribute`


## Benchmark CI gate

- Run locally: `bash ./scripts/run_benchmarks.sh build .benchmark_reports .benchmark_run`
- Reports: `.benchmark_reports/benchmark_report.json` and `.benchmark_reports/benchmark_report.md`
- Thresholds: `benchmark/thresholds.json`

## VSCode extension MVP

The repository includes `extensions/vscode-bridge/` with a no-build VSCode MVP for search, staged session inspection, preview, reject, commit, and recover flows.
