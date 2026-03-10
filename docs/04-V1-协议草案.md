# V1 协议草案

## 握手

- `hello`
- `hello_ack`

## 业务方法

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

## 请求结构

```json
{
  "protocol_version": 1,
  "instance_key": "...",
  "client_id": "cli-001",
  "session_id": "sess-001",
  "request_id": "req-001",
  "method": "workspace.info",
  "params": {
    "workspace_root": "/path/to/ws"
  },
  "options": {
    "timeout_ms": 10000
  }
}
```

## 响应结构

```json
{
  "request_id": "req-001",
  "ok": true,
  "result": {},
  "meta": {
    "duration_ms": 1,
    "truncated": false
  }
}
```

## 诊断字段

`workspace.info` / `workspace.open` 结果中会附带：

- `runtime_dir`
- `platform`
- `transport`

## 主要错误码

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

## 日志与状态

- runtime log：按 runtime 目录落盘，支持按大小轮转
- audit log：按 workspace 的 `.bridge/audit.log` 落盘，支持按大小轮转
- history log：按 workspace 的 `.bridge/history.log` 落盘，支持按大小轮转
- patch previews：按 workspace 的 `.bridge/previews/` 落盘并按策略清理
- patch backups：按 retention count 清理
- audit 字段已增强：`endpoint`、`duration_ms`、`request_bytes`、`response_bytes`、`truncated`

## Timeout / Cancel（当前实现）

- CLI 的 `--timeout-ms` 会转发为服务端处理预算；CLI 传输层还会保留一个小的额外 grace window，以便 structured timeout response 能返回给调用方。
- `search.text` / `search.regex`、streaming `fs.read` / `fs.read_range`、streaming `patch.preview` 均支持 `timeout_ms`。
- `request.cancel` 通过 `target_request_id` 请求取消目标请求。
- 当前 V1 取消能力已在 search、streaming file read、streaming patch preview 路径落地。

## 流式 / 分块响应（当前实现）

- `search.text`、`search.regex`、`fs.read`、`fs.read_range`、`patch.preview` 支持 `stream=true`。
- 流式模式下，服务端会在同一连接上发送多帧响应：
  - `type=chunk`：分块数据
  - `type=final`：最终汇总结果或错误
- CLI 使用 `--stream` 进入流式消费模式；`--json` 时每帧一行 JSON。
- 流式 `type=final` 汇总帧统一包含：
  - `stream_event`
  - `chunk_count`
  - `cancelled`
  - `timed_out`

## Streaming 扩展

- `search.*`：输出 `event=search.match` 的 `type=chunk` 帧，最后输出 `type=final` 汇总帧。
- `fs.read`：输出 `event=fs.read.chunk` 的 `type=chunk` 帧，`data.content` 为文本片段，最后输出包含 `path / encoding / eol / line_count / chunk_count / total_bytes / cancelled / timed_out` 的 `type=final` 帧。
- `fs.read_range`：输出 `event=fs.read_range.chunk` 的 `type=chunk` 帧，最后输出同类 `type=final` 汇总帧。
- `patch.preview`：输出 `event=patch.preview.chunk` 的 `type=chunk` 帧，`data.content` 为 diff 文本片段，最后输出包含 `path / applicable / current_mtime / current_hash / preview_id / preview_created_at / preview_expires_at / chunk_count / total_bytes / cancelled / timed_out` 的 `type=final` 汇总帧。

## Patch preview/apply transaction baseline

- `patch.preview` 返回 `preview_id`、`current_mtime`、`current_hash`、`new_content_hash`、`preview_created_at`、`preview_expires_at`。
- `patch.apply` 支持两种模式：
  - 直接提交 `new_content` + base fields
  - 使用 `preview_id` 执行 apply-after-preview
- 使用 `preview_id` 时，daemon 会装载已保存 preview 内容并校验 preview/base 一致性。
- Preview 状态存储在 `<workspace>/.bridge/previews/`，并按 retention / TTL 策略清理。
- Preview 生命周期错误区分为：
  - `PREVIEW_CONSUMED`
  - `PREVIEW_EXPIRED`
  - `PREVIEW_EVICTED`
  - `PREVIEW_INVALID`
- Patch conflict 仍统一归类为 `PATCH_CONFLICT`，但错误消息会附带更具体的原因，例如：
  - `mtime_changed`
  - `hash_changed`
  - `mtime_and_hash_changed`
- `patch.rollback` 返回恢复后的 `current_hash` / `current_mtime`。
