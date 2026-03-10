$ErrorActionPreference = 'Stop'

param(
  [string]$BuildDir,
  [string]$Config = 'Release',
  [switch]$Rebuild,
  [switch]$RunTests
)

$RepoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $RepoRoot 'tests/helpers/common.ps1')

if (-not $BuildDir) {
  $BuildDir = Join-Path $RepoRoot 'build-win'
}

$daemon = Join-Path $BuildDir "apps\bridge_daemon\$Config\bridge_daemon.exe"
$cli = Join-Path $BuildDir "apps\bridge_cli\$Config\bridge_cli.exe"

if ($Rebuild -or -not (Test-Path $daemon) -or -not (Test-Path $cli)) {
  cmake -S $RepoRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64
  cmake --build $BuildDir --config $Config
}

if ($RunTests) {
  ctest --test-dir $BuildDir -C $Config --output-on-failure
}

$workspace = Join-Path $BuildDir 'smoke workspace 中文'
Remove-Item -Recurse -Force $workspace -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path (Join-Path $workspace 'docs dir') | Out-Null
Set-Content -Path (Join-Path $workspace 'docs dir\smoke 世界.md') -Value "smoke`nbridge`n" -Encoding utf8

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
  Assert-Contains $listText 'docs dir/smoke 世界.md' 'list should include unicode smoke file'

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
