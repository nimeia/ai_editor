# V1 需求与边界

## 目标

构建一个跨平台本地文本工作区桥梁：CLI 向 daemon 发送结构化请求，daemon 在受控 workspace 内返回结果，并通过本地 IPC 暴露给上层 Agent / 工具链。

## 平台目标

- Linux
- macOS
- Windows

当前仓库已同时具备：

- POSIX runtime + Unix Domain Socket 基线
- Windows runtime + Named Pipe 基线
- Windows 原生 PowerShell 集成验证与 smoke 脚本

## 当前 V1 范围

### 核心能力

- 多实例寻址
- 本地 IPC
- `workspace.open`
- `workspace.info`
- `workspace.resolve_path`
- 路径归一化
- workspace containment
- `deny / skip_by_default / normal`
- `fs.list / fs.stat / fs.read / fs.read_range`
- `search.text / search.regex`
- `patch.preview / patch.apply / patch.rollback / history.list`
- `request.cancel`

### 可观测性与工程化

- runtime log
- audit log
- history log + rotation
- backup / preview retention
- 标准错误码
- CLI 人类可读输出 + `--json`
- 流式响应基线
- timeout / cancel 基线

### 已实现的流式覆盖

- search
- file read / read-range
- patch preview

## 当前明确不做

- 索引
- 语义检索
- 远程访问
- 智能三方合并
- 热更新配置
- 多文件原子 patch transaction
- GUI / service manager / installer

## 当前剩余收口点

- P5：发布工程化基线
- P6：Linux + Windows 全量验证与最终发布检查
