# V1 跨平台架构概要

## 设计原则

- Core 保留绝大多数业务逻辑
- Platform Adapters 负责 runtime、锁、endpoint、路径细节与平台差异
- Transport Adapters 负责本地 IPC
- CLI / daemon 与 core 之间保持协议边界清晰，便于后续脚本、Agent 与 UI 接入

## 分层

- `bridge_core`
  - 协议解析 / 序列化
  - 实例模型
  - workspace
  - path policy
  - file/search/patch/logging/error-code services
- `bridge_platform`
  - runtime dir
  - instance lock
  - endpoint resolve
  - POSIX / Windows 平台差异封装
- `bridge_transport`
  - local IPC server/client
  - POSIX: Unix Domain Socket
  - Windows: Named Pipe
- `apps`
  - `bridge_daemon`
  - `bridge_cli`

## 实例模型

`instance_key = hash(user + workspace + profile + policy)`

每个实例持有：

- 独立 endpoint
- 独立 lock
- 独立 runtime_dir

诊断字段通过 `workspace.info` / `workspace.open` 返回：

- `runtime_dir`
- `platform`
- `transport`

## 请求处理链路

1. CLI 组装结构化请求
2. transport 连接对应实例 endpoint
3. daemon 完成握手 / 解析请求
4. core 执行 workspace / file / search / patch 等逻辑
5. daemon 返回单次响应或流式多帧响应
6. CLI 输出为人类可读文本或 `--json` 帧流

## 路径策略

- `deny`: 永远拒绝
- `skip_by_default`: 默认跳过，但显式路径可访问
- `normal`: 正常访问

## 流式 / timeout / cancel 基线

当前实现：

- `search.*` 支持流式结果输出
- `fs.read` / `fs.read_range` 支持流式文本分块输出
- `patch.preview` 支持大 diff 流式输出
- `request.cancel` 已接入 search、streaming file read、streaming patch preview
- `timeout_ms` 已接入 search、streaming file read、streaming patch preview

## Patch 生命周期基线

- `patch.preview` 生成 `preview_id`
- `patch.apply --preview-id` 支持 preview-then-commit
- preview 状态可区分 consumed / expired / evicted / invalid
- rollback 返回恢复后的 hash / mtime
