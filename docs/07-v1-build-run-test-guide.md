# V1 构建运行与测试指南

## 1. 构建

### POSIX

```bash
cmake -S . -B build
cmake --build build -j1
```

### Windows

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## 2. 运行测试

### POSIX 全量测试

```bash
ctest --test-dir build --output-on-failure
```

### Windows 全量测试

```powershell
ctest --test-dir build -C Release --output-on-failure
```

### POSIX 功能测试脚本（按功能域）

```bash
ctest --test-dir build -R 'test_functional_(workspace_ops|workspace_edges|fs_ops|search_ops|patch_lifecycle|stream_cancel_timeout|cancel_edges|logging_release|cli_contract)' --output-on-failure
```

当前已补齐的功能脚本分组：

- workspace 基础链路：`test_functional_workspace_ops`
- workspace 边界与策略：`test_functional_workspace_edges`
- 文本编辑与文件操作：`test_functional_fs_ops`
- 搜索与过滤：`test_functional_search_ops`
- patch 生命周期：`test_functional_patch_lifecycle`
- stream / cancel / timeout：`test_functional_stream_cancel_timeout`
- cancel 边界行为：`test_functional_cancel_edges`
- runtime/audit 日志 + install/cpack 验证：`test_functional_logging_release`
- CLI 合同与输出：`test_functional_cli_contract`

### Windows smoke

```powershell
pwsh ./scripts/windows_smoke.ps1 -BuildDir ./build -Config Release
```

## 3. 手工运行

### 启动 daemon

POSIX:

```bash
./build/apps/bridge_daemon/bridge_daemon --workspace "$PWD"
```

Windows:

```powershell
.\build\apps\bridge_daemon\Release\bridge_daemon.exe --workspace $PWD
```

### 常用 CLI 命令

POSIX:

```bash
./build/apps/bridge_cli/bridge_cli ping --workspace "$PWD"
./build/apps/bridge_cli/bridge_cli info --workspace "$PWD"
./build/apps/bridge_cli/bridge_cli list --workspace "$PWD"
./build/apps/bridge_cli/bridge_cli read --workspace "$PWD" --path README.md
./build/apps/bridge_cli/bridge_cli write --workspace "$PWD" --path demo.txt --content 'hello'
./build/apps/bridge_cli/bridge_cli mkdir --workspace "$PWD" --path notes/archive
./build/apps/bridge_cli/bridge_cli move --workspace "$PWD" --path demo.txt --target-path notes/demo.txt
./build/apps/bridge_cli/bridge_cli copy --workspace "$PWD" --path notes/demo.txt --target-path notes/demo-copy.txt
./build/apps/bridge_cli/bridge_cli rename --workspace "$PWD" --path notes/demo-copy.txt --target-path notes/demo-copy-renamed.txt
./build/apps/bridge_cli/bridge_cli search-text --workspace "$PWD" --query bridge --exts .md,.cpp
```

Windows:

```powershell
.\build\apps\bridge_cli\Release\bridge_cli.exe ping --workspace $PWD
.\build\apps\bridge_cli\Release\bridge_cli.exe info --workspace $PWD
.\build\apps\bridge_cli\Release\bridge_cli.exe list --workspace $PWD
.\build\apps\bridge_cli\Release\bridge_cli.exe read --workspace $PWD --path README.md
.\build\apps\bridge_cli\Release\bridge_cli.exe write --workspace $PWD --path demo.txt --content 'hello'
.\build\apps\bridge_cli\Release\bridge_cli.exe mkdir --workspace $PWD --path notes/archive
.\build\apps\bridge_cli\Release\bridge_cli.exe move --workspace $PWD --path demo.txt --target-path notes/demo.txt
.\build\apps\bridge_cli\Release\bridge_cli.exe copy --workspace $PWD --path notes/demo.txt --target-path notes/demo-copy.txt
.\build\apps\bridge_cli\Release\bridge_cli.exe rename --workspace $PWD --path notes/demo-copy.txt --target-path notes/demo-copy-renamed.txt
.\build\apps\bridge_cli\Release\bridge_cli.exe search-text --workspace $PWD --query bridge --exts .md,.cpp
```

## 4. 重点验证项

- handshake / ping / info / open
- workspace 路径归一化与 containment
- list / stat / read / read-range
- write / mkdir / move / copy / rename
- search text / regex
- stream final summary 字段统一
- timeout / cancel 行为
- patch preview / apply / rollback / history
- Windows Unicode / space path 与 backslash-path normalization
- runtime / audit / history / preview / backup 落盘与清理

## 5. 诊断建议

### 看 runtime 路径

优先通过 `workspace.info` 或 `workspace.open` 返回的 `runtime_dir` 做定位，不要在脚本里硬编码运行时目录。

### 看日志

- runtime log：定位 daemon 运行时错误
- audit log：定位请求级行为和错误码
- history log：定位 patch / rollback 链路

### 关注 timeout / cancel

当前若使用 `--timeout-ms`，CLI 会给 transport 多留一个小缓冲，这样更容易拿到结构化 `REQUEST_TIMEOUT` / `SEARCH_TIMEOUT` 响应，而不是直接连接超时。

## 6. 发布与打包

P5 已补齐安装与归档打包基线，P6 又补了一键全量验证脚本。

- POSIX 打包：`./scripts/package_release.sh --build-dir build --out-dir dist --generator TGZ --run-tests --jobs 1`
- Windows 打包：`pwsh ./scripts/package_release.ps1 -BuildDir build -Config Release -OutDir dist -Generator ZIP -RunTests -Jobs 1`
- POSIX 全量验证：`./scripts/validate_v1.sh --build-dir build --jobs 1`
- Windows 原生功能测试：`pwsh ./tests/functional_windows_native.ps1 <daemon.exe> <bridge_cli.exe> <run_dir>`
- Windows 全量验证：`pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1`

Windows 全量验证脚本现在会在 `.p6_validation_summary/` 下自动生成：

- `windows_validation_summary.json`：结构化机器可读摘要
- `windows_validation_summary.md`：便于人工查看的阶段性结果摘要

即使中途某个阶段失败，上述摘要文件也会写出，便于快速定位失败步骤。在资源受限环境里，建议优先使用 `jobs=1`，以换取更稳定的收尾与打包过程。

更完整的发布说明见 `docs/08-v1-release-and-deployment.md`，本次验证结果见 `docs/09-v1-validation-report.md`。

## 7. GitHub Actions

仓库提供了两条 GitHub Actions 工作流：

- `ci.yml`：日常 CI，覆盖 Linux / macOS / Windows
- `release.yml`：标签发布与手工发布，覆盖 Linux / macOS / Windows，并上传 Release 产物

这两条工作流都直接调用仓库内现有脚本：

- POSIX：`./scripts/validate_v1.sh --build-dir build --config Release --jobs 1`
- Windows：`pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1`

这样本地验证路径与 CI 路径保持一致，减少“本地能过、CI 不过”的分叉问题。
