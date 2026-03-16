# V1 验证报告

日期：2026-03-14

## 1. 结论

当前仓库已经达到 **V1 收尾基线**：

- Linux/POSIX 侧已完成重新配置、重新构建、单元测试、集成测试、功能测试、安装树验证、归档打包验证。
- Windows 侧当前已经具备：
  - GitHub Actions Windows job
  - `tests/functional_windows_native.ps1`
  - `tests/integration_windows_native.ps1`（兼容包装）
  - `scripts/windows_smoke.ps1`
  - Unicode / 空格路径覆盖
  - Named Pipe 原生链路验证脚本
  - stream / cancel / timeout / CLI contract / logging / release package 功能测试矩阵
- 当前仍缺少一次 **原生 Windows 主机复跑**，作为最终发布签收。

## 2. 本次已实际执行的 Linux/POSIX 验证

### 2.1 重新配置与构建

已执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 1
```

说明：

- 在当前受限执行环境里，并行构建的稳定性不如单线程，因此发布与验证脚本新增了 `--jobs` / `-Jobs` 参数，并默认使用 `1`，优先保证收尾阶段的稳定性。

### 2.2 单元测试 + 集成测试 + 功能测试

已执行：

```bash
ctest --test-dir build --output-on-failure
```

本次确认通过的测试覆盖包括：

- shared unit/core tests
- POSIX integration tests
- POSIX functional tests
- `test_integration_patch_stream_ops`
- `test_integration_timeout_ops`
- `test_functional_patch_lifecycle`
- `test_functional_stream_cancel_timeout`
- `test_functional_cancel_edges`
- `test_functional_logging_release`
- `test_functional_cli_contract`

## 3. 本次已实际执行的手工核心链路验证

P6 `scripts/validate_v1.sh` 会自动执行：

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
- install 树内关键文档与二进制是否存在
- 打包输出与 `SHA256SUMS.txt`

## 4. 本次修复与收口

### 4.1 脚本问题修复

已修复 `tests/integration_logging_ops.sh` 的脚本文件格式问题：

- 统一为 LF 行尾，避免在 POSIX shell 下因 CRLF 导致 `set -euo pipefail` 解析异常。
- 保持 `grep -q ... file*` 形式，避免旧式 `cat ... | grep -q ...` 在 `pipefail` 下因 `SIGPIPE` 误判失败。

已同步确认 `scripts/validate_v1.sh` 保持与上述修复一致的稳定写法：

- 归档检测使用 `find ... -print -quit | grep -q .`
- 避免 `pipefail` 下的误报

### 4.2 文档对齐

已对当前代码状态做文档同步：

- README 的核心能力和文本编辑基线已补齐 `fs.move / fs.copy / fs.rename`
- `docs/01-v1-scope-and-boundaries.md` 已补齐文本编辑范围与当前剩余收口点
- `docs/04-v1-protocol.md` 已把 `fs.write / fs.mkdir / fs.move / fs.copy / fs.rename` 纳入正式协议说明
- `docs/06-v1-layout-and-interfaces.md` 已补齐 CLI 命令与测试资产清单
- `docs/07-v1-build-run-test-guide.md` 已修复编码损坏，并补齐 write/mkdir/move/copy/rename 的手工命令与验证项
- `docs/10-v1-release-checklist.md` 已修复编码损坏并与当前验证矩阵对齐

## 5. 安装与归档验证

### 5.1 安装树验证

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

### 5.2 归档打包验证

已验证：

```bash
./scripts/package_release.sh --build-dir build --config Release --out-dir .p6_validation_dist --generator TGZ --jobs 1
```

验证点：

- 归档成功生成
- `SHA256SUMS.txt` 成功生成

## 6. Windows 状态说明

本仓库当前已经具备 Windows 原生验证资产：

- `tests/functional_windows_native.ps1`
- `tests/integration_windows_native.ps1`（兼容包装）
- `scripts/windows_smoke.ps1`
- `tests/functional_windows_stream_cancel_timeout.ps1`
- `tests/functional_windows_cli_contract.ps1`
- `tests/functional_windows_logging_codes.ps1`
- `tests/functional_windows_logging_success.ps1`
- `tests/functional_windows_release_package.ps1`
- GitHub Actions Windows CI job
- Unicode / 空格路径覆盖
- backslash path normalization 覆盖
- runtime/platform/transport 诊断字段验证

建议在最终发布候选上补跑：

```powershell
pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1
```

更推荐直接执行组合签收脚本：

```powershell
pwsh ./scripts/windows_signoff.ps1 -BuildDir build -Config Release -SummaryDir .windows_signoff -Jobs 1 -PromoteToDocs
```

该脚本现已补齐结构化摘要输出，默认会生成：

- `.p6_validation_summary/windows_validation_summary.json`
- `.p6_validation_summary/windows_validation_summary.md`

摘要中会记录 configure / build / ctest / windows_smoke / install / package 各阶段的状态、耗时、安装检查和归档产物列表。

## 7. 剩余事项

当前剩余事项主要是：

- 在原生 Windows 环境复跑 `scripts/validate_v1.ps1`
- 根据实际交付平台决定后续是否继续做：
  - 安装器（MSI/MSIX/PKG/DEB/RPM）
  - 签名 / notarization
  - GitHub Release 自动上传
  - 服务注册 / 自启动
  - 自动更新
