$ErrorActionPreference = 'Stop'

param(
  [string]$Daemon,
  [string]$Cli,
  [string]$RunDir
)

& (Join-Path $PSScriptRoot 'functional_windows_native.ps1') -Daemon $Daemon -Cli $Cli -RunDir $RunDir
exit $LASTEXITCODE
