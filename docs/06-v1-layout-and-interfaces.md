# 鐩綍缁撴瀯涓庢帴鍙ｆ竻鍗?
## 鐩綍

```text
apps/
  bridge_cli/
  bridge_daemon/
libs/
  bridge_core/
  bridge_platform/
  bridge_transport/
tests/
  integration_*.sh
  functional_windows_native.ps1
  integration_windows_native.ps1
  tests_*.cpp
docs/
  01-v1-scope-and-boundaries.md
  02-v1-architecture-overview.md
  03-v1-risks-and-notes.md
  04-v1-protocol.md
  05-v1-plan.md
  06-v1-layout-and-interfaces.md
  07-v1-build-run-test-guide.md
scripts/
  windows_smoke.ps1
```

## 鍏抽敭鎺ュ彛 / 缁勪欢

### Platform

- `bridge::platform::RuntimePaths`
- `bridge::platform::InstanceLock`
- `bridge::platform::GetPlatformName`锛堝钩鍙拌瘖鏂級

### Transport

- `bridge::transport::Server`
- `bridge::transport::Client`
- POSIX Unix Domain Socket adapter
- Windows Named Pipe adapter

### Core

- `bridge::core::WorkspaceConfig`
- `bridge::core::PathPolicy`
- `bridge::core::FileService`
- `bridge::core::SearchService`
- `bridge::core::PatchService`
- `bridge::core::LoggingService`
- `bridge::core::Protocol` / request-response serialization helpers

## CLI 瀵瑰鍛戒护

- `ping`
- `info`
- `open`
- `resolve`
- `list`
- `stat`
- `read`
- `read-range`
- `search-text`
- `search-regex`
- `cancel`
- `patch-preview`
- `patch-apply`
- `patch-rollback`
- `history`

## 娴嬭瘯璧勪骇

### 鍗曞厓娴嬭瘯

- `tests_instance.cpp`
- `tests_path_policy.cpp`
- `tests_file_service.cpp`
- `tests_file_stream.cpp`
- `tests_search_service.cpp`
- `tests_patch_service.cpp`
- `tests_logging.cpp`
- `tests_platform_transport.cpp`

### 闆嗘垚娴嬭瘯

- `integration_ping_info.sh`
- `integration_file_ops.sh`
- `integration_search_ops.sh`
- `integration_patch_ops.sh`
- `integration_logging_ops.sh`
- `integration_cancel_ops.sh`
- `integration_stream_ops.sh`
- `integration_read_stream_ops.sh`
- `integration_patch_stream_ops.sh`
- `integration_timeout_ops.sh`
- `functional_windows_native.ps1`
  - Windows 鍘熺敓鍔熻兘娴嬭瘯鑴氭湰锛岃鐩?workspace / fs / search / patch / log 鍏抽敭璺緞
- `integration_windows_native.ps1`
  - 鍏煎鍖呰鑴氭湰锛岃浆璋?`functional_windows_native.ps1`
