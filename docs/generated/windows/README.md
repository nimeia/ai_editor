# Generated Windows Sign-off Records

这个目录用于存放**真实 Windows 主机**执行后的结构化签收记录。

默认由下面命令生成并回填：

```powershell
pwsh ./scripts/windows_signoff.ps1 -BuildDir build -Config Release -SummaryDir .windows_signoff -Jobs 1 -PromoteToDocs
```

当前仓库内未预置伪造结果；只有在真实 Windows 主机执行后，才应将生成文件提交回仓库。
