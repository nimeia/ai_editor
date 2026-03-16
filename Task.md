# Task.md

## AI 受控文本编辑执行层｜执行任务表

版本：v0.1  
日期：2026-03-14  
适用范围：当前项目从 V1 bridge 基线演进到“搜索 -> 定位 -> 选择 -> staged -> preview -> commit -> rollback”的 AI 受控文本编辑执行层。

---

## 1. 说明

### 1.1 已有基线
当前仓库已经具备以下基础：
- workspace / path containment / 文件读写与目录操作基线
- `search.text` / `search.regex`
- `patch.preview` / `patch.apply` / `patch.rollback` / `preview_id`
- 日志、审计、流式、超时取消、跨平台 CI 基线
- 文本编辑第一轮增强（含 `fs.write` / `fs.mkdir` / move/copy/rename）

### 1.2 本文档的用途
本文档聚焦 **待执行任务**，用于：
- 排定后续研发顺序
- 标明任务间依赖关系
- 统一验收口径
- 作为 `tasks.md` 的上层排期视图

### 1.3 优先级定义
- **P0**：主线阻塞项；不完成则后续关键能力无法稳定推进
- **P1**：核心增强项；完成后产品闭环显著增强
- **P2**：生态与产品化项；不阻塞内核主线，但影响接入与扩展

### 1.4 执行顺序总览
建议主线顺序如下：

`S0 -> E1 -> E2 -> E4 -> E3 -> E5 -> E6 -> E7 -> E8(持续) -> E9A -> E10 -> E9B -> E11`

说明：
- `E8 Benchmark` 在 `E1` 后即可启动骨架，并伴随主线持续补样本与门禁。
- `E9A` 先做 Markdown / JSON；`E9B` 再做 YAML / HTML。
- `E10 SDK` 在 `E4/E5/E6` 稳定后启动最合适。

---

## 2. Epic 依赖图

| Epic | 名称 | 直接依赖 | 备注 |
|---|---|---|---|
| S0 | V1 基线冻结 | 无 | 进入新主线前的收口 |
| E1 | 领域模型与协议基线 | S0 | 后续所有核心能力前置 |
| E2 | Search v2 与命中对象标准化 | E1 | 为 selector / session / preview 提供统一输入 |
| E3 | Selector / Locator / Rebase | E2 | 为 block 级编辑与恢复提供定位能力 |
| E4 | Session v1 骨架 | E1 | 为 staged / preview / commit / recover 提供事务骨架 |
| E5 | 编辑原语第一批 | E3, E4 | 主线真实可用编辑能力 |
| E6 | Inspect / Preview / Risk v1 | E4, E5 | 人机可读审查闭环 |
| E7 | Recovery / Conflict / Rollback v2 | E3, E4, E6 | 稳定性与可恢复性核心 |
| E8 | Benchmark 与评测框架 | E1 | 应持续跟随主线演进 |
| E9A | 结构适配器 MVP（Markdown / JSON） | E3, E5, E6 | 第一批结构化编辑能力 |
| E9B | 结构适配器扩展（YAML / HTML） | E9A | 第二批结构化编辑能力 |
| E10 | SDK / Agent Contract | E4, E5, E6 | 对外接入层基线 |
| E11 | VSCode 插件 MVP | E7, E10 | 产品化接入验证 |

---

## 3. 可执行任务表

## S0｜V1 基线冻结

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| S0 | S0-T01 | 在真实 Windows 主机完成 P6 native rerun，并补齐验证记录 | 无 | 输出 Windows 原生执行记录、失败项说明、通过项清单；能复现实验命令 | P0 |
| S0 | S0-T02 | 生成 V1 baseline validation report（Linux / macOS / Windows） | S0-T01 | 形成一份跨平台基线报告；列出已支持命令、限制、已知风险 | P0 |
| S0 | S0-T03 | 打 `v1-baseline` 标签并冻结兼容基线 | S0-T02 | 仓库可定位到冻结点；README / docs / tasks 状态一致 | P0 |
| S0 | S0-T04 | 从基线标签切出“controlled-editing”主线分支与文档入口 | S0-T03 | 新主线分支创建完成；文档中明确后续能力在新分支推进 | P0 |

## E1｜领域模型与协议基线

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E1 | E1-T01 | 定义核心领域对象：`Match` / `Selector` / `Scope` / `RiskHint` / `StagedChange` / `CommitRecord` | S0-T04 | 形成统一头文件/模型定义；字段命名、序列化、错误码口径统一 | P0 |
| E1 | E1-T02 | 设计协议版本策略与 JSON schema 目录布局 | E1-T01 | 新旧 schema 可区分；协议版本字段、兼容策略文档落地 | P0 |
| E1 | E1-T03 | 设计 `session.*` / `edit.*` / `recovery.*` 命令族合同 | E1-T01, E1-T02 | 命令输入输出字段固定；危险动作默认受控；错误可机器读取 | P0 |
| E1 | E1-T04 | 明确 `patch.*` 到 `session/edit.*` 的兼容与迁移策略 | E1-T03 | 文档中写清保留、兼容、废弃路径；无未决冲突接口 | P0 |
| E1 | E1-T05 | 增加协议/模型 contract tests 基线 | E1-T02, E1-T03 | schema 校验、序列化回归、错误码回归可自动跑通 | P0 |

## E2｜Search v2 与命中对象标准化

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E2 | E2-T01 | 升级搜索命中对象到 `Match v2`，统一 `search.text` / `search.regex` 输出结构 | E1-T05 | 两类搜索返回相同主结构；含 `match_id` / `line_range` / `excerpt` / `anchor` 等字段 | P0 |
| E2 | E2-T02 | 增加目录、扩展名、文件类型、行范围等过滤能力 | E2-T01 | 过滤条件可组合；结果数量与命中范围符合预期 | P0 |
| E2 | E2-T03 | 增加 `selector_reason` / `confidence` / 初版排序规则 | E2-T01 | 搜索结果具备推荐原因与置信度；多命中时排序可解释 | P0 |
| E2 | E2-T04 | 预留 `scope` / `block_type` / `symbol-like` 元数据扩展位 | E2-T01 | 不破坏现有协议；为后续 selector / structure adapter 预留字段 | P1 |
| E2 | E2-T05 | 完成 Search v2 单测、集成测试与回归夹具 | E2-T02, E2-T03, E2-T04 | 多命中/高相似文本/过滤场景测试通过 | P0 |

## E3｜Selector / Locator / Rebase

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E3 | E3-T01 | 实现选择器基线：第 N 个 / 最后一个 / 限定文件 / 限定目录 / 限定扩展名 | E2-T05 | 可用统一 selector 规则准确选中目标 match | P0 |
| E3 | E3-T02 | 实现 anchor-based locator（before/after/nearest） | E2-T05 | 多个相似命中场景下可借锚点正确消歧 | P0 |
| E3 | E3-T03 | 实现 locator validate / rebase / drift 容错基线 | E3-T01, E3-T02 | 小范围外部改动后仍能重定位；失败时明确说明原因 | P0 |
| E3 | E3-T04 | 定义歧义、失配、低置信度等失败分类与错误码 | E3-T01, E3-T02 | 不发生静默误替换；失败响应机器可读、人可理解 | P0 |
| E3 | E3-T05 | 增加 selector / locator 专项测试与真实样本回归 | E3-T03, E3-T04 | 高相似文本、重复段落、偏移漂移场景通过率可统计 | P0 |

## E4｜Session v1 骨架

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E4 | E4-T01 | 设计 session 存储布局、元数据与生命周期状态机 | E1-T05 | session 目录结构固定；状态至少覆盖 created/staged/previewed/committed/aborted/recoverable | P0 |
| E4 | E4-T02 | 实现 `session.begin` / `session.add` / `session.abort` | E4-T01 | 可创建会话、向会话添加 staged change、可中止并清理 | P0 |
| E4 | E4-T03 | 实现 `session.inspect` / `session.preview` 基线 | E4-T02 | 可查看 staged 摘要与 preview 结果；按文件聚合展示 | P0 |
| E4 | E4-T04 | 实现 `session.commit`，并复用/接管现有 `patch.preview/apply` 底座 | E4-T03 | commit 走受控链路；旧 patch 能力未被破坏 | P0 |
| E4 | E4-T05 | 实现 `session.snapshot` / `session.recover` 最小可用版 | E4-T02 | 中断后可恢复 staged 状态；可识别缺失/损坏快照 | P0 |
| E4 | E4-T06 | 完成 session 系列合同测试、异常状态测试、跨平台回归 | E4-T04, E4-T05 | 生命周期与异常路径自动测试通过 | P0 |

## E5｜编辑原语第一批

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E5 | E5-T01 | 实现 `edit.replace_range`，接入 `session.add` | E4-T06 | 可对显式范围做受控替换；支持 preview 与 commit | P0 |
| E5 | E5-T02 | 实现 `edit.replace_block`，接入 selector / locator | E3-T05, E4-T06 | 可按 block/anchor/selector 选择并替换目标块 | P0 |
| E5 | E5-T03 | 实现 `edit.insert_before` / `edit.insert_after` | E3-T05, E4-T06 | 插入位置由 selector / anchor 决定；预览与提交一致 | P0 |
| E5 | E5-T04 | 实现 `edit.delete_block` | E3-T05, E4-T06 | 删除操作默认受控；失败时给出块损坏/未命中等原因 | P0 |
| E5 | E5-T05 | 统一编辑失败分类：未命中 / 歧义 / 冲突 / 风险拒绝 / 块损坏 | E5-T01, E5-T02, E5-T03, E5-T04 | 所有编辑原语返回统一错误结构 | P0 |
| E5 | E5-T06 | 完成编辑原语集成测试与多文件 staged 回归 | E5-T05 | 单文件/多文件/多命中/错误路径场景通过 | P0 |

## E6｜Inspect / Preview / Risk v1

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E6 | E6-T01 | 设计 `session.inspect` 摘要结构：文件数、块数、selector reason、anchor、risk | E4-T06, E5-T06 | inspect 输出可同时服务 CLI / SDK / IDE | P0 |
| E6 | E6-T02 | 设计 `session.preview` 机器视图（结构化 hunk / diff / metadata） | E6-T01 | preview 输出可被 SDK/IDE 消费，无需二次猜测 | P0 |
| E6 | E6-T03 | 设计 `session.preview` 人类视图（摘要、关键变更、风险提示） | E6-T01 | 用户无需通读完整 diff 即可判断修改可信度 | P0 |
| E6 | E6-T04 | 实现 risk scoring v1：多命中、低置信度、大块替换、跨文件高影响提示 | E6-T01, E3-T05 | 风险级别与原因明确；高风险默认显式提示 | P0 |
| E6 | E6-T05 | 完成 CLI 展示与 JSON 输出对齐 | E6-T02, E6-T03, E6-T04 | `--json` 与 human-readable 输出一致；信息不丢失 | P1 |
| E6 | E6-T06 | 完成 inspect/preview/risk 测试与示例文档 | E6-T05 | 回归测试通过；README / protocol 示例更新 | P0 |

## E7｜Recovery / Conflict / Rollback v2

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E7 | E7-T01 | 实现外部改动冲突检测（mtime/hash/content overlap） | E3-T05, E4-T06 | commit/recover 前可发现冲突；冲突类型可区分 | P0 |
| E7 | E7-T02 | 实现 `recovery.check` / `recovery.rebase` 基线 | E7-T01 | 可判断 session 是否还能继续；支持可控 rebase | P0 |
| E7 | E7-T03 | 强化 `session.recover` 状态机与错误路径 | E4-T05, E7-T02 | 中断、损坏、外部改动、部分提交等路径可处理 | P0 |
| E7 | E7-T04 | 补齐 commit 状态机：validating / writing / verifying / recorded | E4-T04, E7-T01 | commit 中间态可观测；失败不会留下不可解释状态 | P0 |
| E7 | E7-T05 | 校验 rollback / audit / commit record 一致性 | E7-T03, E7-T04 | 回滚后审计记录、文件状态、commit record 一致 | P0 |
| E7 | E7-T06 | 完成 recovery/conflict/rollback 专项测试 | E7-T05 | 回滚成功率与恢复成功率可量化；异常路径可复现 | P0 |

## E8｜Benchmark 与评测框架（持续）

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E8 | E8-T01 | 建 benchmark 目录结构、场景定义格式与 runner 骨架 | E1-T05 | 可描述样本、执行命令、采集结果与统计指标 | P0 |
| E8 | E8-T02 | 建立“高相似文本 / 多命中”样本集 | E8-T01, E2-T05 | 能统计命中正确率、误改率、漏改率 | P0 |
| E8 | E8-T03 | 建立“漂移恢复 / rebase”样本集 | E8-T01, E3-T05, E7-T06 | 能统计 recover/rebase 成功率 | P0 |
| E8 | E8-T04 | 建立“多文件 staged / inspect / preview / commit”样本集 | E8-T01, E6-T06 | 能统计一次通过率与 preview 接受率 | P0 |
| E8 | E8-T05 | 输出 benchmark report 与阈值门禁建议 | E8-T02, E8-T03, E8-T04 | 报告包含趋势、失败样本、阈值建议 | P1 |
| E8 | E8-T06 | 将关键 benchmark 接入 CI 或发布前检查 | E8-T05 | 关键指标低于阈值时可阻断合并或发布 | P1 |

## E9A｜结构适配器 MVP（Markdown / JSON）

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E9A | E9A-T01 | 设计 Markdown section / heading / list item 的选择模型 | E3-T05, E5-T06 | 可稳定表示 Markdown 块选择目标 | P1 |
| E9A | E9A-T02 | 实现 Markdown `replace_section` / `insert_after_heading` / `upsert_section` | E9A-T01, E6-T06 | 常见文档编辑动作可走结构化原语完成 | P1 |
| E9A | E9A-T03 | 设计 JSON key-path / array-item 的选择模型 | E3-T05, E5-T06 | JSON 路径选择可稳定表达对象/数组节点 | P1 |
| E9A | E9A-T04 | 实现 JSON `replace_value` / `upsert_key` / `append_array_item` | E9A-T03, E6-T06 | JSON 常见修改可不依赖纯文本匹配 | P1 |
| E9A | E9A-T05 | 完成 Markdown / JSON 结构适配器测试与文档示例 | E9A-T02, E9A-T04 | 示例、单测、集成测试齐全 | P1 |

## E9B｜结构适配器扩展（YAML / HTML）

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E9B | E9B-T01 | 设计 YAML key-path / map-array 的选择模型 | E9A-T05 | YAML 节点选择具备确定性表示 | P2 |
| E9B | E9B-T02 | 实现 YAML `replace_value` / `upsert_key` / `append_item` | E9B-T01 | 常见 YAML 修改可走结构化原语完成 | P2 |
| E9B | E9B-T03 | 设计 HTML node / attribute / sibling 的选择模型 | E9A-T05 | 基础 HTML 节点选择可表达插入/替换目标 | P2 |
| E9B | E9B-T04 | 实现 HTML `replace_node` / `insert_after_node` / `set_attribute` | E9B-T03 | 基础 HTML 编辑能力跑通 | P2 |
| E9B | E9B-T05 | 完成 YAML / HTML 结构适配器测试与文档示例 | E9B-T02, E9B-T04 | 结构编辑回归可自动执行 | P2 |

## E10｜SDK / Agent Contract

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E10 | E10-T01 | 设计 Python SDK typed API 与错误模型映射 | E6-T06, E7-T06 | Python 侧可稳定调用 session/edit/recovery 主链 | P1 |
| E10 | E10-T02 | 实现 Python SDK、示例脚本与最小集成测试 | E10-T01 | 示例场景可跑通；错误处理与文档完整 | P1 |
| E10 | E10-T03 | 设计 TypeScript SDK typed API 与错误模型映射 | E6-T06, E7-T06 | TS 侧接口与 Python 口径一致 | P1 |
| E10 | E10-T04 | 实现 TypeScript SDK、示例脚本与最小集成测试 | E10-T03 | 示例场景可跑通；与协议版本兼容 | P1 |
| E10 | E10-T05 | 输出 Agent contract、版本兼容与接入示例文档 | E10-T02, E10-T04 | AI/CLI/IDE 接入方可据此实现调用链 | P1 |

## E11｜VSCode 插件 MVP

| Epic | Task ID | 任务 | 依赖 | 验收标准 | 优先级 |
|---|---|---|---|---|---|
| E11 | E11-T01 | 搭建 VSCode 扩展骨架，接入 SDK / transport | E10-T05 | 扩展能连接到本地执行层并完成基础调用 | P2 |
| E11 | E11-T02 | 实现搜索结果视图与 staged 变更面板 | E11-T01 | 用户可查看 match、选择目标、查看 staged 文件列表 | P2 |
| E11 | E11-T03 | 实现 preview / accept / reject / commit 流程 | E11-T02, E7-T06 | 用户可按文件或变更块接受/拒绝后提交 | P2 |
| E11 | E11-T04 | 实现 session recover、risk 提示与错误可视化 | E11-T03 | IDE 重启后可恢复会话；高风险改动有明确提示 | P2 |
| E11 | E11-T05 | 完成 VSCode MVP 端到端验证与演示文档 | E11-T04 | 可完成一条完整编辑链路演示；文档可指导复现 | P2 |

---

## 4. 推荐迭代切分

### Iter 0：基线冻结
- S0-T01 ~ S0-T04

### Iter 1：协议与模型
- E1-T01 ~ E1-T05
- E8-T01

### Iter 2：Search v2 + Session 骨架起步
- E2-T01 ~ E2-T05
- E4-T01 ~ E4-T03

### Iter 3：Selector / Locator + Session commit/recover
- E3-T01 ~ E3-T05
- E4-T04 ~ E4-T06

### Iter 4：编辑原语第一批
- E5-T01 ~ E5-T06
- E6-T01 ~ E6-T03

### Iter 5：Risk / Recovery / Rollback v2
- E6-T04 ~ E6-T06
- E7-T01 ~ E7-T06
- E8-T02 ~ E8-T04

### Iter 6：结构适配器 MVP + Python SDK
- E9A-T01 ~ E9A-T05
- E10-T01 ~ E10-T02

### Iter 7：TS SDK + 结构适配器扩展
- E10-T03 ~ E10-T05
- E9B-T01 ~ E9B-T05

### Iter 8：VSCode MVP + 收口
- E11-T01 ~ E11-T05
- E8-T05 ~ E8-T06

---

## 5. 每个 Epic 的完成定义（Definition of Done）

任一 Epic 完成前，至少满足：
- 代码实现完成
- 单元测试完成
- 集成测试完成
- 回归样本补齐
- README / protocol / Task.md / tasks.md 更新
- 至少一条真实工作流可演示
- 若该 Epic 已纳入 benchmark，则 benchmark 指标达到阶段门槛

---

## 6. 当前建议立即执行的任务串

建议下一步直接进入以下主线：

1. `S0-T01 ~ S0-T04`
2. `E1-T01 ~ E1-T05`
3. `E2-T01 ~ E2-T05`
4. `E4-T01 ~ E4-T06`
5. `E3-T01 ~ E3-T05`
6. `E5-T01 ~ E5-T06`

原因：
- 这条链最直接补齐“受控编辑执行层”的核心抽象。
- 现有 `patch.preview/apply/rollback` 能作为 `session` 底座复用。
- 若跳过该链直接做结构适配器或 SDK，后续大概率会返工。



## Update 2026-03-15

- E7 baseline advanced: added `recovery.check`, `recovery.rebase`, commit conflict/rebase gate, and recover summary counters.
- E8 skeleton added: `benchmark/runner.py` and starter scenarios for high-similarity search and session rebase.
