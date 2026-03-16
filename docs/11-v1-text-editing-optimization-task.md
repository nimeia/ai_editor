# V1 文本编辑优化任务

## 目标

把桥接工具从“只读 + patch 修改已有文件”为主，推进到更接近真实文本编辑器的基线：

- 支持直接写文本文件
- 支持显式创建目录
- 支持对不存在的目标文件做 patch preview / apply
- 保持 rollback / history / CLI contract 的一致性
- 改善 patch preview 的 diff 可读性

## 本轮已完成

### T1 `fs.write`

- 新增 `fs.write` 方法
- CLI 新增 `write` 命令
- 默认自动创建父目录
- 返回 `created` / `parent_created` / `bytes_written` 等结果字段

### T2 `fs.mkdir`

- 新增 `fs.mkdir` 方法
- CLI 新增 `mkdir` 命令
- 默认递归创建目录

### T3 `patch.preview` diff 收窄

- 从近似整文件替换，调整为单个上下文 hunk 输出
- 小范围文本改动更易审查

### T4 测试补强

已补或已更新：

- `test_file_service_edges`
- `test_patch_service_edges`
- `test_patch_service`
- `test_integration_file_ops`
- `test_integration_patch_ops`
- `test_functional_fs_ops`
- `test_functional_patch_lifecycle`
- `test_functional_cli_contract`

### T5 patch 对不存在文件的正式支持

- `patch.preview` 允许目标文件不存在
- `patch.apply` 允许创建新文件
- `patch.rollback` 对“原本不存在、后来创建”的文件会恢复为不存在
- backup 清理逻辑已兼容 `.bak + .meta`

### T6 诊断与错误归类补充

新增或补充归类：

- `ALREADY_EXISTS`
- `FILE_NOT_FOUND`（父目录不存在场景）
- `INVALID_PARAMS`（编码 / eol / 非文本内容 / 目录冲突等）

## 当前结论

对“文本编辑”这个定位而言，本轮补齐后主链路已经更完整：

- 读：`fs.list` / `fs.stat` / `fs.read` / `fs.read_range`
- 写：`fs.write` / `fs.mkdir`
- 变更：`patch.preview` / `patch.apply` / `patch.rollback` / `history.list`

## 后续可继续完善

- `fs.write` 的显式编码 / BOM / EOL 参数在 CLI 层开放
- `fs.write` 的 overwrite 策略在 CLI 层开放
- patch diff 从“单 hunk 收窄”进一步演进到“多 hunk”
- patch / write 的错误码再做更细分层


## Round 2 completed

- Added formal `fs.move`, `fs.copy`, and `fs.rename` operations.
- Added CLI commands: `move`, `copy`, `rename`.
- Added protocol result payloads and error-code normalization for new file operations.
- Added POSIX functional coverage and updated Windows CLI contract script.
