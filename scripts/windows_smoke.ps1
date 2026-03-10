param(
  [string]$BuildDir,
  [string]$Config = 'Release',
  [switch]$Rebuild,
  [switch]$RunTests
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $RepoRoot 'tests/helpers/common.ps1')

function Get-BuildExecutablePath {
  param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$TargetSubdir,
    [Parameter(Mandatory = $true)][string]$ExeName,
    [Parameter(Mandatory = $true)][string]$Config
  )

  $multiConfigPath = Join-Path $BuildDir "$TargetSubdir\$Config\$ExeName"
  if (Test-Path $multiConfigPath) {
    return $multiConfigPath
  }

  return (Join-Path $BuildDir "$TargetSubdir\$ExeName")
}

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

if (-not $BuildDir) {
  $BuildDir = Join-Path $RepoRoot 'build-win'
}

if (Test-Path $BuildDir) {
  $BuildDir = (Resolve-Path $BuildDir).Path
} else {
  $BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
}

$daemon = Get-BuildExecutablePath -BuildDir $BuildDir -TargetSubdir 'apps\bridge_daemon' -ExeName 'bridge_daemon.exe' -Config $Config
$cli = Get-BuildExecutablePath -BuildDir $BuildDir -TargetSubdir 'apps\bridge_cli' -ExeName 'bridge_cli.exe' -Config $Config

if ($Rebuild -or -not (Test-Path $daemon) -or -not (Test-Path $cli)) {
  $generator = Get-CmakeGenerator -BuildDir $BuildDir
  if (-not $generator) {
    cmake -S $RepoRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64
  }
  cmake --build $BuildDir --config $Config

  $daemon = Get-BuildExecutablePath -BuildDir $BuildDir -TargetSubdir 'apps\bridge_daemon' -ExeName 'bridge_daemon.exe' -Config $Config
  $cli = Get-BuildExecutablePath -BuildDir $BuildDir -TargetSubdir 'apps\bridge_cli' -ExeName 'bridge_cli.exe' -Config $Config
}

if (-not (Test-Path $daemon) -or -not (Test-Path $cli)) {
  throw "missing build outputs: daemon=$daemon cli=$cli"
}

$daemon = (Resolve-Path $daemon).Path
$cli = (Resolve-Path $cli).Path

if ($RunTests) {
  ctest --test-dir $BuildDir -C $Config --output-on-failure
}

$workspace = Join-Path $BuildDir 'smoke-workspace'
$relativeSmokeFile = 'docs dir\smoke-file.md'
Remove-Item -Recurse -Force $workspace -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path (Join-Path $workspace 'docs dir') | Out-Null
Set-Content -Path (Join-Path $workspace $relativeSmokeFile) -Value "smoke`nbridge`n" -Encoding utf8

Initialize-BridgeTest -Daemon $daemon -Cli $cli -RunDir $BuildDir -Workspace $workspace
Start-BridgeDaemon -Name 'windows_smoke'
try {
  $pingText = Invoke-BridgeCli @('ping', '--workspace', $workspace, '--json')
  Assert-Contains $pingText '"ok":true' 'ping should succeed'

  $infoResult = Invoke-BridgeCliJson @('info', '--workspace', $workspace, '--json')
  Set-BridgeRuntimeDirFromInfo -InfoObject $infoResult.Json
  Assert-JsonEq $infoResult.Json 'result.platform' 'windows' 'info should report windows platform'
  Assert-JsonEq $infoResult.Json 'result.transport' 'windows-named-pipe' 'info should report named pipe transport'

  $listText = Invoke-BridgeCli @('list', '--workspace', $workspace, '--json', '--recursive')
  Assert-Contains $listText 'docs dir/smoke-file.md' 'list should include smoke file'

  Write-Host "Windows smoke OK"
  Write-Host "  workspace: $workspace"
  Write-Host "  endpoint : $($infoResult.Json.result.endpoint)"
  Write-Host "  runtime  : $($infoResult.Json.result.runtime_dir)"
}
catch {
  Show-BridgeDiagnostics
  throw
}
finally {
  Stop-BridgeDaemon
}

exit 0
