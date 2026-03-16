# V1 发布与部署指南

## 目标

当前发布基线已经从“只有压缩包”扩展到“压缩包 + 平台原生安装包”：

- 明确版本号与产物命名
- 支持 `cmake --install` 安装布局
- 支持 `cpack` 生成 archive 与 native installer
- 提供 Linux/macOS/Windows 的一键打包脚本
- 让运行时产物保持精简，只保留实际运行所需文件

## 版本与产物命名

当前仓库版本来自根 `CMakeLists.txt`：

- `project(ai_bridge VERSION 0.1.0 LANGUAGES CXX)`

归档产物命名规则：

- Linux archive：`ai_bridge-<version>-linux-<arch>.tar.gz`
- Linux installer：`ai_bridge_<version>_<arch>.deb`
- macOS archive：`ai_bridge-<version>-macos-<arch>.tar.gz`
- macOS installer：`ai_bridge-<version>-macos-<arch>.pkg`
- Windows archive：`ai_bridge-<version>-windows-<arch>.zip`
- Windows installer：`ai_bridge-<version>-windows-<arch>.exe`

其中：

- `<platform>` 目前取 `linux` / `macos` / `windows`
- `<arch>` 取 CMake 的 `CMAKE_SYSTEM_PROCESSOR` 小写结果，例如 `x86_64` / `amd64` / `arm64`

## 安装布局

通过 `cmake --install` 安装后的默认运行时布局：

```text
<prefix>/
  bin/
    bridge_daemon(.exe)
    bridge_cli(.exe)
  share/ai_bridge/
    README.md
```

说明：

- 当前 runtime package 只保留运行时文件
- 不再把内部验证文档、发布脚本、开发头文件打进最终安装包
- 平台原生安装包已接入：Linux `DEB`、macOS `PKG`、Windows `NSIS`

## 版本信息

`bridge_cli` 与 `bridge_daemon` 现在支持：

```bash
bridge_cli --version
bridge_daemon --version
```

输出包含：

- 软件版本
- 平台族 (`posix` / `windows`)
- 传输族 (`unix-domain-socket` / `windows-named-pipe`)

## 打包脚本

### Linux/macOS 风格

```bash
./scripts/package_release.sh --build-dir build --out-dir dist --jobs 1 --run-tests
```

常用参数：

- `--build-dir <dir>`：构建目录
- `--config <cfg>`：构建配置，默认 `Release`
- `--out-dir <dir>`：产物输出目录，默认 `dist`
- `--generator <gen>` / `--generators <g1,g2,...>`：显式指定一个或多个 CPack 生成器；默认按平台输出 archive + native installer
- `--run-tests`：打包前先跑 CTest
- `--jobs <n>`：构建并行度，默认 `1`

### Windows PowerShell

```powershell
pwsh ./scripts/package_release.ps1 -BuildDir build -Config Release -OutDir dist -RunTests -Jobs 1
```

## 发布产物内容

运行时产物默认包含：

- `bin/bridge_daemon`
- `bin/bridge_cli`
- `share/ai_bridge/README.md`

不再包含：

- `include/**` 开发头文件
- `share/ai_bridge/docs/**` 内部验证文档
- `share/ai_bridge/scripts/**` 打包/验证脚本

脚本会额外在输出目录生成：

- `SHA256SUMS.txt`

用于记录 archive 与 native installer 的 SHA-256 校验值。

当通过 GitHub Actions `release.yml` 聚合三个平台的产物时，工作流会把它们重命名为：

- `SHA256SUMS-linux.txt`
- `SHA256SUMS-macos.txt`
- `SHA256SUMS-windows.txt`

这样可以避免多平台产物下载到同一发布页时发生同名覆盖。

## 运行时目录说明

发布产物安装后，运行时目录仍由程序在本机按平台规则生成，而不是安装到固定系统目录：

- POSIX：`${XDG_RUNTIME_DIR:-/tmp/ai_bridge_runtime}/<uid>/...`
- Windows：通过 `workspace.info` / `workspace.open` 返回的 `runtime_dir` 诊断字段定位

工作区状态文件仍写入目标 workspace：

- `<workspace>/.bridge/audit.log`
- `<workspace>/.bridge/history.log`
- `<workspace>/.bridge/previews/`
- `<workspace>/.bridge/backups/`

## 最小发布验收

建议每个发布候选至少执行：

### POSIX

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
./scripts/package_release.sh --build-dir build --out-dir dist --jobs 1
```

### Windows

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
pwsh ./scripts/windows_smoke.ps1 -BuildDir ./build -Config Release
pwsh ./scripts/package_release.ps1 -BuildDir build -Config Release -OutDir dist
```

## 当前边界

当前发布链路已经具备 archive + native installer，但仍未覆盖：

- 服务注册或开机自启
- 升级器 / auto-update
- 平台签名 / notarization
- 更丰富的 Linux 安装格式（RPM 等）

后续阶段仍可继续扩展签名、notarization、自动升级与服务注册。

## P6 验证脚本

P6 新增端到端验证脚本，用于把“构建、测试、安装、打包、核心链路抽查”串成一次可重复执行的验证。

### POSIX

```bash
./scripts/validate_v1.sh --build-dir build --jobs 1
```

### Windows

```powershell
pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1
```

建议在最终发布候选上先跑验证脚本，再决定是否进入正式发布。

## 8. GitHub Actions 建议配置

仓库现在建议使用两条工作流：

### 8.1 `ci.yml`

用途：日常提交与 PR 校验。

建议触发：

- `push`
- `pull_request`
- `workflow_dispatch`

建议内容：

- Linux：执行 `./scripts/validate_v1.sh --build-dir build --config Release --jobs 1`
- macOS：执行 `./scripts/validate_v1.sh --build-dir build --config Release --jobs 1`
- Windows：执行 `pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1`
- 将 `.p6_validation_dist/`、`.p6_validation_install/`、`.p6_validation_summary/`、测试输出目录上传为 artifact

### 8.2 `release.yml`

用途：标签发布与手工发布。

建议触发：

- `push tags: v*`
- `workflow_dispatch`

建议内容：

- 三个平台重新执行完整验证脚本，而不是只跑 `ctest`
- 上传 `.p6_validation_dist/` 中的归档产物
- 手工触发时必须 checkout 到输入的 `tag` / `ref` 对应代码，再进行构建与发布，避免“发布页 tag 与构建代码不一致”
- 聚合 job 下载 artifact 时保留平台子目录，避免同名文件被覆盖
- 最终由一个聚合 job 下载所有 artifact 并发布到 GitHub Release
- 手工触发时允许设置：
  - `tag`
  - `draft`
  - `prerelease`

### 8.3 工作流文件

当前仓库已提供：

- `.github/workflows/ci.yml`
- `.github/workflows/release.yml`

它们会直接复用仓库已有的验证与打包脚本，避免 CI 流程和本地流程分叉。
