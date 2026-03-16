param(
  [string]$BuildDir = "build",
  [string]$Config = "Release",
  [string]$WorkspaceDir = ".p6_validation_workspace",
  [string]$InstallDir = ".p6_validation_install",
  [string]$DistDir = ".p6_validation_dist",
  [string]$SummaryDir = ".windows_signoff",
  [int]$Jobs = 1,
  [switch]$PromoteToDocs
)

$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path -LiteralPath '.').Path
$PowerShellExe = (Get-Command pwsh -ErrorAction SilentlyContinue)?.Source
if (-not $PowerShellExe) { $PowerShellExe = (Get-Command powershell).Source }

function Ensure-Dir([string]$Path) {
  New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Read-JsonFile([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path)) {
    throw "missing json file: $Path"
  }
  return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -Depth 100
}

function To-Bool($Value) {
  if ($null -eq $Value) { return $false }
  if ($Value -is [bool]) { return $Value }
  return [System.Convert]::ToBoolean($Value)
}

Ensure-Dir $SummaryDir
$validationSummaryDir = Join-Path $SummaryDir 'validation'
$rerunSummaryDir = Join-Path $SummaryDir 'native_rerun'
Ensure-Dir $validationSummaryDir
Ensure-Dir $rerunSummaryDir

& $PowerShellExe -File ./scripts/validate_v1.ps1 `
  -BuildDir $BuildDir `
  -Config $Config `
  -WorkspaceDir $WorkspaceDir `
  -InstallDir $InstallDir `
  -DistDir $DistDir `
  -SummaryDir $validationSummaryDir `
  -Jobs $Jobs

& $PowerShellExe -File ./scripts/windows_native_rerun_record.ps1 `
  -BuildDir $BuildDir `
  -Config $Config `
  -ReportDir $rerunSummaryDir `
  -Jobs $Jobs

$validationJsonPath = Join-Path $validationSummaryDir 'windows_validation_summary.json'
$rerunJsonPath = Join-Path $rerunSummaryDir 'windows_native_rerun_record.json'
$validation = Read-JsonFile $validationJsonPath
$rerun = Read-JsonFile $rerunJsonPath

$combined = [ordered]@{
  script = 'scripts/windows_signoff.ps1'
  started_at = $validation.started_at
  finished_at = (Get-Date).ToUniversalTime().ToString('o')
  repo_root = $RepoRoot
  build_dir = (Join-Path $RepoRoot $BuildDir)
  config = $Config
  validation_ok = (To-Bool $validation.ok)
  rerun_status = [string]$rerun.status
  ok = ((To-Bool $validation.ok) -and ([string]$rerun.status -eq 'pass'))
  validation_summary_json = $validationJsonPath
  rerun_summary_json = $rerunJsonPath
  validation_summary_md = (Join-Path $validationSummaryDir 'windows_validation_summary.md')
  rerun_summary_md = (Join-Path $rerunSummaryDir 'windows_native_rerun_record.md')
  notes = @(
    'This combined summary is intended to be generated on a real Windows host.',
    'A successful sign-off requires both validate_v1.ps1 and windows_native_rerun_record.ps1 to pass.'
  )
}

$combinedJsonPath = Join-Path $SummaryDir 'windows_signoff_summary.json'
$combinedMdPath = Join-Path $SummaryDir 'windows_signoff_summary.md'
$combined | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $combinedJsonPath -Encoding utf8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('# Windows Sign-off Summary')
$lines.Add('')
$lines.Add("- Status: $(if ($combined.ok) { 'PASS' } else { 'FAIL' })")
$lines.Add("- Validation OK: $($combined.validation_ok)")
$lines.Add("- Native rerun status: $($combined.rerun_status)")
$lines.Add("- Started (UTC): $($combined.started_at)")
$lines.Add("- Finished (UTC): $($combined.finished_at)")
$lines.Add('')
$lines.Add('## Outputs')
$lines.Add('')
$lines.Add("- Validation JSON: $($combined.validation_summary_json)")
$lines.Add("- Validation MD: $($combined.validation_summary_md)")
$lines.Add("- Native rerun JSON: $($combined.rerun_summary_json)")
$lines.Add("- Native rerun MD: $($combined.rerun_summary_md)")
$lines.Add('')
$lines.Add('## Notes')
$lines.Add('')
foreach ($note in $combined.notes) {
  $lines.Add("- $note")
}
$lines -join "`r`n" | Set-Content -LiteralPath $combinedMdPath -Encoding utf8

if ($PromoteToDocs) {
  $docsDir = Join-Path $RepoRoot 'docs/generated/windows'
  Ensure-Dir $docsDir
  Copy-Item -LiteralPath $validationJsonPath -Destination (Join-Path $docsDir 'windows_validation_summary.json') -Force
  Copy-Item -LiteralPath (Join-Path $validationSummaryDir 'windows_validation_summary.md') -Destination (Join-Path $docsDir 'windows_validation_summary.md') -Force
  Copy-Item -LiteralPath $rerunJsonPath -Destination (Join-Path $docsDir 'windows_native_rerun_record.json') -Force
  Copy-Item -LiteralPath (Join-Path $rerunSummaryDir 'windows_native_rerun_record.md') -Destination (Join-Path $docsDir 'windows_native_rerun_record.md') -Force
  Copy-Item -LiteralPath $combinedJsonPath -Destination (Join-Path $docsDir 'windows_signoff_summary.json') -Force
  Copy-Item -LiteralPath $combinedMdPath -Destination (Join-Path $docsDir 'windows_signoff_summary.md') -Force
}

Write-Host "Windows sign-off JSON: $combinedJsonPath"
Write-Host "Windows sign-off MD  : $combinedMdPath"
if (-not $combined.ok) {
  exit 1
}
