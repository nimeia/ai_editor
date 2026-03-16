# 目录结构与接口清单

## 目录

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
  functional_*.sh
  functional_windows_*.ps1
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
  08-v1-release-and-deployment.md
  09-v1-validation-report.md
  10-v1-release-checklist.md
  11-v1-text-editing-optimization-task.md
scripts/
  package_release.sh
  package_release.ps1
  validate_v1.sh
  validate_v1.ps1
  windows_smoke.ps1
```

## 关键接口 / 组件

### Platform

- `bridge::platform::RuntimePaths`
- `bridge::platform::InstanceLock`
- `bridge::platform::GetPlatformName`（平台诊断）

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

## CLI 对外命令

- `ping`
- `info`
- `open`
- `resolve`
- `list`
- `stat`
- `read`
- `read-range`
- `write`
- `mkdir`
- `move`
- `copy`
- `rename`
- `search-text`
- `search-regex`
- `cancel`
- `patch-preview`
- `patch-apply`
- `patch-rollback`
- `history`

## 测试资产

### 单元测试

- `tests_instance.cpp`
- `tests_path_policy.cpp`
- `tests_file_service.cpp`
- `tests_file_stream.cpp`
- `tests_search_service.cpp`
- `tests_patch_service.cpp`
- `tests_logging.cpp`
- `tests_platform_transport.cpp`
- `tests_error_codes.cpp`
- `tests_file_service_edges.cpp`
- `tests_search_service_edges.cpp`
- `tests_patch_service_edges.cpp`

### POSIX 集成 / 功能测试

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
- `functional_workspace_ops.sh`
- `functional_workspace_edges.sh`
- `functional_fs_ops.sh`
- `functional_search_ops.sh`
- `functional_patch_lifecycle.sh`
- `functional_stream_cancel_timeout.sh`
- `functional_cancel_edges.sh`
- `functional_logging_release.sh`
- `functional_cli_contract.sh`

### Windows 测试与脚本

- `functional_windows_native.ps1`
  - Windows 原生功能测试脚本，覆盖 workspace / fs / search / patch / logging / Unicode 路径
- `integration_windows_native.ps1`
  - 兼容包装脚本，转调 `functional_windows_native.ps1`
- `functional_windows_stream_cancel_timeout.ps1`
- `functional_windows_cli_contract.ps1`
- `functional_windows_logging_codes.ps1`
- `functional_windows_logging_success.ps1`
- `functional_windows_release_package.ps1`
- `windows_smoke.ps1`
  - Windows 快速烟测脚本，用于复用现有 build 输出做最小链路验证
