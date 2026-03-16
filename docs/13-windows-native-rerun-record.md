# Windows Native Rerun Record

Status: **pending external execution on a real Windows host**

This repository now includes `scripts/windows_native_rerun_record.ps1` to produce a structured rerun record for the native Windows sign-off step.

## Intended execution

```powershell
pwsh ./scripts/windows_native_rerun_record.ps1 -BuildDir build -Config Release -ReportDir .windows_native_rerun -Jobs 1
```

## Current state inside the sandbox

The sandbox cannot act as a real Windows host, so this document is added as a handoff record rather than a completed rerun result.


## Recommended combined sign-off command

```powershell
pwsh ./scripts/windows_signoff.ps1 -BuildDir build -Config Release -SummaryDir .windows_signoff -Jobs 1 -PromoteToDocs
```
