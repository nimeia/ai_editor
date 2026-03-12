param(
  [string]$Daemon,
  [string]$Cli,
  [string]$RunDir
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'helpers/common.ps1')

$RepoRoot = Split-Path -Parent $PSScriptRoot
$InstallDir = Join-Path $RunDir 'install_functional_windows'
$DistDir = Join-Path $RunDir 'dist_functional_windows'
$ExtractDir = Join-Path $RunDir 'extract_functional_windows'
$SmokeWorkspace = Join-Path $RunDir 'ws functional windows release package'

function Get-BuildDirFromExecutablePath {
  param([string]$Path)
  $full = [System.IO.Path]::GetFullPath($Path)
  $parent = Split-Path -Parent $full
  foreach ($name in @('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel')) {
    if ((Split-Path -Leaf $parent) -eq $name) {
      return (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $parent)))
    }
  }
  return (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $full)))
}

function Get-BuildConfigFromExecutablePath {
  param([string]$Path)
  foreach ($name in @('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel')) {
    if ($Path -match ([regex]::Escape([IO.Path]::DirectorySeparatorChar + $name + [IO.Path]::DirectorySeparatorChar))) {
      return $name
    }
    if ($Path -match ([regex]::Escape('/' + $name + '/'))) {
      return $name
    }
  }
  return 'Release'
}

function Assert-PathExists {
  param([string]$Path, [string]$Message)
  if (-not (Test-Path $Path)) {
    throw "$Message`nMissing: $Path"
  }
}

$Config = Get-BuildConfigFromExecutablePath -Path $Daemon
$BuildDir = Get-BuildDirFromExecutablePath -Path $Daemon

Remove-Item -Recurse -Force $InstallDir, $DistDir, $ExtractDir, $SmokeWorkspace -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $SmokeWorkspace 'docs dir') | Out-Null
Set-Content -Path (Join-Path $SmokeWorkspace 'docs dir\pkg-smoke.md') -Value "package`nwindows`nbridge`n" -Encoding utf8

try {
  cmake --install $BuildDir --config $Config --prefix $InstallDir | Out-Null

  $installedDaemon = Join-Path $InstallDir 'bin/bridge_daemon.exe'
  $installedCli = Join-Path $InstallDir 'bin/bridge_cli.exe'
  $installedReadme = Join-Path $InstallDir 'share/ai_bridge/README.md'
  $installedValidation = Join-Path $InstallDir 'share/ai_bridge/docs/09-v1-validation-report.md'
  $installedChecklist = Join-Path $InstallDir 'share/ai_bridge/docs/10-v1-release-checklist.md'
  $installedSmokeScript = Join-Path $InstallDir 'share/ai_bridge/scripts/windows_smoke.ps1'

  Assert-PathExists $installedDaemon 'installed daemon should exist'
  Assert-PathExists $installedCli 'installed cli should exist'
  Assert-PathExists $installedReadme 'installed README should exist'
  Assert-PathExists $installedValidation 'installed validation report should exist'
  Assert-PathExists $installedChecklist 'installed release checklist should exist'
  Assert-PathExists $installedSmokeScript 'installed smoke script should exist'

  $installedCliVersion = & $installedCli --version
  if ($LASTEXITCODE -ne 0) { throw 'installed bridge_cli --version failed' }
  Assert-Contains $installedCliVersion 'ai_bridge_cli ' 'installed bridge_cli should report version'

  $installedDaemonVersion = & $installedDaemon --version
  if ($LASTEXITCODE -ne 0) { throw 'installed bridge_daemon --version failed' }
  Assert-Contains $installedDaemonVersion 'ai_bridge_daemon ' 'installed bridge_daemon should report version'

  Initialize-BridgeTest -Daemon $installedDaemon -Cli $installedCli -RunDir $RunDir -Workspace $SmokeWorkspace
  Start-BridgeDaemon -Name 'functional_windows_release_package'
  try {
    $pingText = Invoke-BridgeCli @('ping', '--workspace', $SmokeWorkspace, '--json')
    Assert-Contains $pingText '"ok":true' 'installed binaries should answer ping'

    $infoResult = Invoke-BridgeCliJson @('info', '--workspace', $SmokeWorkspace, '--json')
    Set-BridgeRuntimeDirFromInfo -InfoObject $infoResult.Json
    Assert-JsonEq $infoResult.Json 'result.platform' 'windows' 'installed daemon should report windows platform'
    Assert-JsonEq $infoResult.Json 'result.transport' 'windows-named-pipe' 'installed daemon should report named pipe transport'

    $listText = Invoke-BridgeCli @('list', '--workspace', $SmokeWorkspace, '--json', '--recursive')
    Assert-Contains $listText 'docs dir/pkg-smoke.md' 'installed binaries should list smoke file'
  }
  finally {
    Stop-BridgeDaemon
  }

  Push-Location $RepoRoot
  try {
    & (Join-Path $RepoRoot 'scripts/package_release.ps1') -BuildDir $BuildDir -Config $Config -OutDir $DistDir -Generator ZIP -Jobs 1
  }
  finally {
    Pop-Location
  }

  $hashFile = Join-Path $DistDir 'SHA256SUMS.txt'
  Assert-PathExists $hashFile 'package_release.ps1 should generate SHA256SUMS.txt'
  $hashText = Get-Content $hashFile -Raw
  Assert-Contains $hashText 'ai_bridge-' 'checksum file should mention ai_bridge archive'

  $archives = Get-ChildItem -Path $DistDir -File | Where-Object { $_.Extension -eq '.zip' } | Sort-Object Name
  if ($archives.Count -lt 1) {
    throw 'expected at least one ZIP archive from package_release.ps1'
  }

  $archive = $archives[0]
  Expand-Archive -Path $archive.FullName -DestinationPath $ExtractDir -Force

  $packagedDaemon = Get-ChildItem -Path $ExtractDir -Recurse -Filter 'bridge_daemon.exe' | Select-Object -First 1
  $packagedCli = Get-ChildItem -Path $ExtractDir -Recurse -Filter 'bridge_cli.exe' | Select-Object -First 1
  $packagedReadme = Get-ChildItem -Path $ExtractDir -Recurse -Filter 'README.md' | Where-Object { $_.FullName -match 'share[\\/]ai_bridge' } | Select-Object -First 1
  $packagedValidation = Get-ChildItem -Path $ExtractDir -Recurse -Filter '09-v1-validation-report.md' | Select-Object -First 1
  $packagedChecklist = Get-ChildItem -Path $ExtractDir -Recurse -Filter '10-v1-release-checklist.md' | Select-Object -First 1
  $packagedSmoke = Get-ChildItem -Path $ExtractDir -Recurse -Filter 'windows_smoke.ps1' | Select-Object -First 1

  if ($null -eq $packagedDaemon) { throw 'packaged archive missing bridge_daemon.exe' }
  if ($null -eq $packagedCli) { throw 'packaged archive missing bridge_cli.exe' }
  if ($null -eq $packagedReadme) { throw 'packaged archive missing README.md under share/ai_bridge' }
  if ($null -eq $packagedValidation) { throw 'packaged archive missing validation report' }
  if ($null -eq $packagedChecklist) { throw 'packaged archive missing release checklist' }
  if ($null -eq $packagedSmoke) { throw 'packaged archive missing windows_smoke.ps1' }
}
catch {
  Show-BridgeDiagnostics
  throw
}

exit 0
