param(
  [string]$BuildDir = "build",
  [string]$Config = "Release",
  [string]$WorkspaceDir = ".p6_validation_workspace",
  [string]$InstallDir = ".p6_validation_install",
  [string]$DistDir = ".p6_validation_dist",
  [string]$SummaryDir = ".p6_validation_summary",
  [int]$Jobs = 1
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path -LiteralPath ".").Path

function Get-CmakeGenerator {
  param([string]$BuildDir)

  $cachePath = Join-Path $BuildDir 'CMakeCache.txt'
  if (-not (Test-Path $cachePath)) {
    return $null
  }

  $line = Select-String -Path $cachePath -Pattern '^CMAKE_GENERATOR:INTERNAL=' | Select-Object -First 1
  if ($null -eq $line) {
    return $null
  }

  return ($line.Line -replace '^CMAKE_GENERATOR:INTERNAL=', '')
}

function Get-PowerShellExe {
  $cmd = Get-Command pwsh -ErrorAction SilentlyContinue
  if ($cmd) {
    return $cmd.Source
  }
  $cmd = Get-Command powershell -ErrorAction SilentlyContinue
  if ($cmd) {
    return $cmd.Source
  }
  throw 'neither pwsh nor powershell is available'
}

$PowerShellExe = Get-PowerShellExe
$script:ValidationSummary = [ordered]@{
  script = "scripts/validate_v1.ps1"
  platform = "windows"
  started_at = (Get-Date).ToUniversalTime().ToString("o")
  finished_at = $null
  ok = $false
  build_dir = (Join-Path $RepoRoot $BuildDir)
  config = $Config
  workspace_dir = (Join-Path $RepoRoot $WorkspaceDir)
  install_dir = (Join-Path $RepoRoot $InstallDir)
  dist_dir = (Join-Path $RepoRoot $DistDir)
  summary_dir = (Join-Path $RepoRoot $SummaryDir)
  host = [ordered]@{
    computer_name = $env:COMPUTERNAME
    user_name = $env:USERNAME
    powershell = $PSVersionTable.PSVersion.ToString()
    os = [System.Environment]::OSVersion.VersionString
  }
  steps = @()
  artifacts = @()
  checksum_file = $null
  install_checks = @()
  notes = @()
}

function Ensure-Directory {
  param([string]$Path)
  New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Add-ValidationNote {
  param([string]$Message)
  $script:ValidationSummary.notes += $Message
}

function Invoke-ValidationStep {
  param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][scriptblock]$Action
  )

  Write-Host "==> $Name"
  $started = Get-Date
  try {
    & $Action
    $ended = Get-Date
    $script:ValidationSummary.steps += [ordered]@{
      name = $Name
      ok = $true
      started_at = $started.ToUniversalTime().ToString("o")
      finished_at = $ended.ToUniversalTime().ToString("o")
      duration_ms = [math]::Round(($ended - $started).TotalMilliseconds)
      message = "ok"
    }
  }
  catch {
    $ended = Get-Date
    $script:ValidationSummary.steps += [ordered]@{
      name = $Name
      ok = $false
      started_at = $started.ToUniversalTime().ToString("o")
      finished_at = $ended.ToUniversalTime().ToString("o")
      duration_ms = [math]::Round(($ended - $started).TotalMilliseconds)
      message = $_.Exception.Message
    }
    throw
  }
}

function Update-ValidationArtifacts {
  $artifactFiles = @()
  if (Test-Path $DistDir) {
    $artifactFiles = Get-ChildItem -Path $DistDir -File | Sort-Object Name
  }
  $script:ValidationSummary.artifacts = @(
    $artifactFiles | ForEach-Object {
      [ordered]@{
        name = $_.Name
        path = $_.FullName
        size_bytes = $_.Length
        last_write_time_utc = $_.LastWriteTimeUtc.ToString("o")
      }
    }
  )
  $checksumFile = Join-Path $DistDir "SHA256SUMS.txt"
  if (Test-Path $checksumFile) {
    $script:ValidationSummary.checksum_file = $checksumFile
  }
}

function Update-InstallChecks {
  $checks = @(
    [ordered]@{ name = 'bridge_daemon'; path = (Join-Path $InstallDir 'bin/bridge_daemon.exe') },
    [ordered]@{ name = 'bridge_cli'; path = (Join-Path $InstallDir 'bin/bridge_cli.exe') },
    [ordered]@{ name = 'README'; path = (Join-Path $InstallDir 'share/ai_bridge/README.md') },
    [ordered]@{ name = 'validation_report'; path = (Join-Path $InstallDir 'share/ai_bridge/docs/09-v1-validation-report.md') },
    [ordered]@{ name = 'release_checklist'; path = (Join-Path $InstallDir 'share/ai_bridge/docs/10-v1-release-checklist.md') }
  )
  $script:ValidationSummary.install_checks = @(
    $checks | ForEach-Object {
      [ordered]@{
        name = $_.name
        path = $_.path
        ok = (Test-Path $_.path)
      }
    }
  )
}

function Write-ValidationSummary {
  Ensure-Directory $SummaryDir
  $script:ValidationSummary.finished_at = (Get-Date).ToUniversalTime().ToString("o")
  $jsonPath = Join-Path $SummaryDir 'windows_validation_summary.json'
  $mdPath = Join-Path $SummaryDir 'windows_validation_summary.md'

  $json = $script:ValidationSummary | ConvertTo-Json -Depth 8
  Set-Content -Path $jsonPath -Value $json -Encoding utf8

  $lines = [System.Collections.Generic.List[string]]::new()
  $lines.Add('# Windows Validation Summary')
  $lines.Add('')
  $lines.Add("- Status: " + ($(if ($script:ValidationSummary.ok) { 'PASS' } else { 'FAIL' })))
  $lines.Add("- Started (UTC): $($script:ValidationSummary.started_at)")
  $lines.Add("- Finished (UTC): $($script:ValidationSummary.finished_at)")
  $lines.Add("- BuildDir: $($script:ValidationSummary.build_dir)")
  $lines.Add("- Config: $($script:ValidationSummary.config)")
  $lines.Add("- InstallDir: $($script:ValidationSummary.install_dir)")
  $lines.Add("- DistDir: $($script:ValidationSummary.dist_dir)")
  $lines.Add("- SummaryDir: $($script:ValidationSummary.summary_dir)")
  $lines.Add('')
  $lines.Add('## Steps')
  $lines.Add('')
  foreach ($step in $script:ValidationSummary.steps) {
    $status = if ($step.ok) { 'PASS' } else { 'FAIL' }
    $lines.Add("- [$status] $($step.name) ($($step.duration_ms) ms): $($step.message)")
  }
  $lines.Add('')
  $lines.Add('## Install Checks')
  $lines.Add('')
  foreach ($check in $script:ValidationSummary.install_checks) {
    $status = if ($check.ok) { 'PASS' } else { 'FAIL' }
    $lines.Add("- [$status] $($check.name): $($check.path)")
  }
  $lines.Add('')
  $lines.Add('## Artifacts')
  $lines.Add('')
  if ($script:ValidationSummary.artifacts.Count -eq 0) {
    $lines.Add('- none')
  } else {
    foreach ($artifact in $script:ValidationSummary.artifacts) {
      $lines.Add("- $($artifact.name) ($($artifact.size_bytes) bytes): $($artifact.path)")
    }
  }
  if ($script:ValidationSummary.checksum_file) {
    $lines.Add('')
    $lines.Add("- Checksum file: $($script:ValidationSummary.checksum_file)")
  }
  if ($script:ValidationSummary.notes.Count -gt 0) {
    $lines.Add('')
    $lines.Add('## Notes')
    $lines.Add('')
    foreach ($note in $script:ValidationSummary.notes) {
      $lines.Add("- $note")
    }
  }
  Set-Content -Path $mdPath -Value ($lines -join "`r`n") -Encoding utf8

  Write-Host "Validation summary JSON: $jsonPath"
  Write-Host "Validation summary MD  : $mdPath"
}

try {
  Add-ValidationNote "The summary files are generated even when a validation step fails."
  Add-ValidationNote "Run this script on a real Windows host to complete native sign-off."

  Invoke-ValidationStep -Name 'configure' -Action {
    $generator = Get-CmakeGenerator -BuildDir $BuildDir
    if (-not $generator) {
      cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64
    }
  }

  Invoke-ValidationStep -Name 'build' -Action {
    cmake --build $BuildDir --config $Config --parallel $Jobs
  }

  Invoke-ValidationStep -Name 'ctest' -Action {
    ctest --test-dir $BuildDir -C $Config --output-on-failure
  }

  Invoke-ValidationStep -Name 'windows_smoke' -Action {
    & $PowerShellExe -File ./scripts/windows_smoke.ps1 -BuildDir $BuildDir -Config $Config
  }

  Invoke-ValidationStep -Name 'prepare_output_dirs' -Action {
    if (Test-Path $InstallDir) { Remove-Item $InstallDir -Recurse -Force }
    if (Test-Path $DistDir) { Remove-Item $DistDir -Recurse -Force }
    if (Test-Path $SummaryDir) { Remove-Item $SummaryDir -Recurse -Force }
  }

  Invoke-ValidationStep -Name 'install' -Action {
    cmake --install $BuildDir --config $Config --prefix $InstallDir
    Update-InstallChecks
    foreach ($check in $script:ValidationSummary.install_checks) {
      if (-not $check.ok) {
        throw "install check failed: $($check.name) => $($check.path)"
      }
    }
  }

  Invoke-ValidationStep -Name 'package' -Action {
    & $PowerShellExe -File ./scripts/package_release.ps1 -BuildDir $BuildDir -Config $Config -OutDir $DistDir -Generator ZIP -Jobs $Jobs
    Update-ValidationArtifacts
    if (-not $script:ValidationSummary.checksum_file) {
      throw 'SHA256SUMS.txt missing'
    }
    if ($script:ValidationSummary.artifacts.Count -eq 0) {
      throw 'no release artifacts produced'
    }
  }

  $script:ValidationSummary.ok = $true
  Write-Host 'P6 validation complete'
}
finally {
  Update-InstallChecks
  Update-ValidationArtifacts
  Write-ValidationSummary
}
