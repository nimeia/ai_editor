# Windows Native Sign-off Runbook

目标：在**真实 Windows 主机**上完成 V1/controlled-editing 当前基线的最终原生签收，并把结果固化为可归档的结构化记录。

## 一键执行

```powershell
pwsh ./scripts/windows_signoff.ps1 -BuildDir build -Config Release -SummaryDir .windows_signoff -Jobs 1 -PromoteToDocs
```

该命令会依次执行：

1. `scripts/validate_v1.ps1`
2. `scripts/windows_native_rerun_record.ps1`
3. 生成组合摘要：
   - `.windows_signoff/windows_signoff_summary.json`
   - `.windows_signoff/windows_signoff_summary.md`
4. 传 `-PromoteToDocs` 时，将结果复制到：
   - `docs/generated/windows/`

## 输出清单

- `docs/generated/windows/windows_validation_summary.json`
- `docs/generated/windows/windows_validation_summary.md`
- `docs/generated/windows/windows_native_rerun_record.json`
- `docs/generated/windows/windows_native_rerun_record.md`
- `docs/generated/windows/windows_signoff_summary.json`
- `docs/generated/windows/windows_signoff_summary.md`

## 通过标准

同时满足以下条件才算签收完成：

- `windows_validation_summary.json` 中 `ok == true`
- `windows_native_rerun_record.json` 中 `status == "pass"`
- `windows_signoff_summary.json` 中 `ok == true`

可用下面命令快速检查：

```bash
python3 ./scripts/check_windows_signoff.py .windows_signoff
```

## 建议回填到仓库的内容

1. 将 `docs/generated/windows/*` 提交到仓库。
2. 把 `tasks.md` 中的 `P6 Windows native rerun on a real Windows host` 改为已完成。
3. 在 `docs/09-v1-validation-report.md` 增加本次实机执行日期与机器环境。

## 当前沙盒状态

当前打包产物已包含全部仓库侧自动化与模板，但**未包含真实 Windows 实机执行结果**。
