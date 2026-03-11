# V1 验证报告

日期：2026-03-09

## 1. 结论

当前仓库已经达到 **V1 收尾基线**：

- Linux/POSIX 侧已在本次 P6 中完成重新配置、重新构建、单元测试、集成测试、手工核心链路验证、安装树验证、归档打包验证。
- Windows 侧当前已经具备：
  - GitHub Actions Windows job
  - `tests/functional_windows_native.ps1`
  - `tests/integration_windows_native.ps1`（兼容包装）
  - `scripts/windows_smoke.ps1`
  - Unicode / 空格路径覆盖
  - Named Pipe 原生链路验证脚本
- 但本次 P6 所在执行环境为 Linux 容器，**未直接执行 Windows 原生验证**。因此，Windows 部分在本报告中记为“已有验证资产，待在原生 Windows 环境复跑确认”。

## 2. 本次已实际执行的 Linux/POSIX 验证

### 2.1 重新配置与构建

已执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 1
```

说明：

- 在当前受限执行环境里，并行构建的稳定性不如单线程，因此发布与验证脚本新增了 `--jobs` / `-Jobs` 参数，并默认使用 `1`，优先保证收尾阶段的稳定性。

### 2.2 单元测试 + 集成测试

已执行：

```bash
ctest --test-dir build --output-on-failure
```

本次确认通过的测试覆盖：

- `test_instance`
- `test_path_policy`
- `test_file_service`
- `test_file_stream`
- `test_search_service`
- `test_patch_service`
- `test_platform_transport`
- `test_logging`
- `test_bridge_cli_version`
- `test_bridge_daemon_version`
- `test_integration_ping_info`
- `test_integration_file_ops`
- `test_integration_search_ops`
- `test_integration_patch_ops`
- `test_integration_logging_ops`
- `test_integration_cancel_ops`
- `test_integration_stream_ops`
- `test_integration_read_stream_ops`

此外，较慢的两项流式/超时链路测试也已单独复跑通过：

- `tests/integration_patch_stream_ops.sh`
- `tests/integration_timeout_ops.sh`

## 3. 本次已实际执行的手工核心链路验证

P6 新增 `scripts/validate_v1.sh`，会自动执行：

- `ping`
- `info`
- `open`
- `list`
- `stat`
- `read`
- `read-range`
- `search-text`
- `search-regex`
- `read --stream`
- `patch-preview`
- `patch-apply`
- `history`
- `patch-rollback`

同时验证：

- `workspace.info` 里的 `runtime_dir / platform / transport`
- `.bridge/audit.log`
- `.bridge/history.log`

## 4. 安装与归档验证

本次 P6 已把安装和归档也纳入验证范围。

### 4.1 安装树验证

已验证：

```bash
cmake --install build --prefix .p6_validation_install
```

安装树中至少确认了：

- `bin/bridge_daemon`
- `bin/bridge_cli`
- `share/ai_bridge/README.md`
- `share/ai_bridge/docs/09-v1-validation-report.md`
- `share/ai_bridge/docs/10-v1-release-checklist.md`

### 4.2 归档打包验证

已验证：

```bash
./scripts/package_release.sh --build-dir build --config Release --out-dir .p6_validation_dist --generator TGZ --jobs 1
```

验证点：

- 归档成功生成
- `SHA256SUMS.txt` 成功生成

## 5. Windows 状态说明

本仓库当前已经具备 Windows 原生验证资产：

- `tests/functional_windows_native.ps1`
- `tests/integration_windows_native.ps1`（兼容包装）
- `scripts/windows_smoke.ps1`
- GitHub Actions Windows CI job
- Unicode / 空格路径覆盖
- backslash path normalization 覆盖
- runtime/platform/transport 诊断字段验证

本次 P6 未在当前环境直接执行 Windows 验证，因此建议在最终发布候选上补跑：

```powershell
pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1
```

该脚本现已补齐结构化摘要输出，默认会生成：

- `.p6_validation_summary/windows_validation_summary.json`
- `.p6_validation_summary/windows_validation_summary.md`

摘要中会记录 configure / build / ctest / windows_smoke / install / package 各阶段的状态、耗时、安装检查和归档产物列表。

## 6. 本次收尾阶段顺手修复

P6 顺手补了一项发布工程化的小收口：

- `scripts/package_release.sh`
- `scripts/package_release.ps1`

新增：

- `--jobs <n>` / `-Jobs <n>`

这样在资源受限环境下可以稳定使用 `1`，减少发布验证阶段因并行构建导致的不稳定。

## 7. 剩余事项

当前剩余事项已经很少，主要是：

- 在原生 Windows 环境复跑 `scripts/validate_v1.ps1`
- 根据实际交付平台决定后续是否继续做：
  - 安装器（MSI/MSIX/PKG/DEB/RPM）
  - 签名 / notarization
  - GitHub Release 自动上传
  - 服务注册 / 自启动
  - 自动更新


## 8. 发布链路收口补充

在 P6 收尾后，发布链路又补了一轮工程化修正：

- `release.yml` 手工触发时会显式 checkout 到输入的 `tag/ref`，再执行构建与发版
- 多平台 checksum 在 GitHub Release 中会使用平台独立文件名，避免 `SHA256SUMS.txt` 冲突覆盖
- 仓库内文档源文件名已恢复为规范 UTF-8 中文文件名，与 `CMakeLists.txt` 安装引用一致

这些修正降低了“发布名称正确但内容不是目标 tag”以及“安装阶段找不到文档源文件”的风险。

## 9. 2026-03-10 全量复查补充

本次又做了一轮更细的全量复查，重点覆盖：

- 干净目录重新 configure
- 重新构建 `bridge_core / bridge_platform / bridge_transport / bridge_daemon / bridge_cli`
- 重新构建测试目标并分段执行全部 26 项测试
- 重新执行安装树验证与 TGZ 归档验证

本轮复查确认并修复了两项脚本稳定性问题：

1. `tests/integration_logging_ops.sh`
   - 原先在 `set -euo pipefail` 下使用 `cat ... | grep -q ...`
   - 当 `grep -q` 提前命中退出时，`cat` 可能收到 `SIGPIPE`，从而把本应通过的测试误判为失败
   - 现已改为直接对匹配文件执行 `grep -q ... file*`

2. `scripts/validate_v1.sh`
   - 原先用 `find ... | grep -q .` 判断归档是否生成
   - 在 `pipefail` 下也存在被 `SIGPIPE` 影响的潜在误判
   - 现已改为 `find ... -print -quit | grep -q .`，避免误报

这轮复查后，Linux/POSIX 侧的构建、测试、安装、归档链路一致性进一步提升；Windows 侧仍建议在原生环境按既定脚本完成最终签发验证。

