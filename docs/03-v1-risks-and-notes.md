# V1 风险与注意事项

## 已识别风险

1. 路径归一化错误会导致 workspace 越界判断失真
2. 不同平台路径语义不同，不能把 Windows 规则写死进 core
3. 大结果如果没有截断策略，后续 `fs.read/search` 会放大风险
4. runtime 目录、socket/pipe 权限、实例锁必须下沉到平台层
5. 软排除目录只能默认跳过，不能等同于硬禁止

## V1 对策

- 路径检查统一走 `PathPolicy`
- endpoint 与 lock 统一走平台层
- 协议字段固定：`client_id/session_id/request_id`
- 先做小闭环，按 M1 -> M5 递进
