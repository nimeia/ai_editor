$ErrorActionPreference = 'Stop'

$script:BridgeDaemonProc = $null
$script:BridgeDaemonStdoutLog = $null
$script:BridgeDaemonStderrLog = $null
$script:BridgeRuntimeDir = $null
$script:BridgeWorkspace = $null
$script:BridgeCli = $null
$script:BridgeDaemon = $null
$script:BridgeRunDir = $null

function Write-BridgeLog {
  param([string]$Message)
  Write-Host "[bridge-test] $Message"
}

function Get-JsonValue {
  param(
    [Parameter(Mandatory = $true)] $InputObject,
    [Parameter(Mandatory = $true)][string]$Path
  )

  $current = $InputObject
  foreach ($segment in ($Path -split '\.')) {
    if ($segment -match '^(?<name>[^\[]+)(\[(?<index>\d+)\])?$') {
      $name = $Matches['name']
      if ($null -eq $current) { return $null }
      if ($current -is [System.Collections.IDictionary]) {
        $current = $current[$name]
      } else {
        $property = $current.PSObject.Properties[$name]
        if ($null -eq $property) { return $null }
        $current = $property.Value
      }
      if ($Matches.ContainsKey('index') -and -not [string]::IsNullOrEmpty($Matches['index'])) {
        $index = [int]$Matches['index']
        if ($null -eq $current -or $current.Count -le $index) { return $null }
        $current = $current[$index]
      }
    } else {
      throw "unsupported json path segment: $segment"
    }
  }
  return $current
}

function Assert-True {
  param([bool]$Condition, [string]$Message)
  if (-not $Condition) {
    throw $Message
  }
}

function Assert-Contains {
  param([string]$Text, [string]$Needle, [string]$Message)
  if (-not $Text.Contains($Needle)) {
    throw "$Message`nExpected to find: $Needle`nActual: $Text"
  }
}

function Assert-NotContains {
  param([string]$Text, [string]$Needle, [string]$Message)
  if ($Text.Contains($Needle)) {
    throw "$Message`nDid not expect to find: $Needle`nActual: $Text"
  }
}

function Assert-JsonEq {
  param($JsonObject, [string]$Path, [string]$Expected, [string]$Message)
  $actual = [string](Get-JsonValue -InputObject $JsonObject -Path $Path)
  if ($actual -ne $Expected) {
    throw "$Message`nExpected: $Expected`nActual  : $actual`
Path    : $Path"
  }
}

function Assert-JsonContains {
  param($JsonObject, [string]$Path, [string]$Needle, [string]$Message)
  $actual = [string](Get-JsonValue -InputObject $JsonObject -Path $Path)
  if (-not $actual.Contains($Needle)) {
    throw "$Message`nExpected to find: $Needle`nActual: $actual`
Path: $Path"
  }
}

function Assert-JsonTruthy {
  param($JsonObject, [string]$Path, [string]$Message)
  $actual = Get-JsonValue -InputObject $JsonObject -Path $Path
  if (-not $actual) {
    throw "$Message`nPath: $Path"
  }
}

function Assert-JsonLenGe {
  param($JsonObject, [string]$Path, [int]$Minimum, [string]$Message)
  $actual = Get-JsonValue -InputObject $JsonObject -Path $Path
  $count = if ($null -eq $actual) { 0 } elseif ($actual -is [string]) { $actual.Length } elseif ($actual -is [System.Collections.ICollection]) { $actual.Count } else { @($actual).Count }
  if ($count -lt $Minimum) {
    throw "$Message`nExpected length >= $Minimum but got $count`nPath: $Path"
  }
}

function Invoke-BridgeCli {
  param([string[]]$CommandArgs)
  $output = & $script:BridgeCli @CommandArgs
  if ($LASTEXITCODE -ne 0) {
    $joinedOutput = if ($null -eq $output) { '' } else { [string]::Join("`n", $output) }
    throw "bridge_cli failed: $($CommandArgs -join ' ')`n$joinedOutput"
  }
  if ($null -eq $output) {
    return ''
  }
  return [string]::Join("`n", $output)
}

function Invoke-BridgeCliAllowFail {
  param([string[]]$CommandArgs)
  $output = & $script:BridgeCli @CommandArgs 2>&1
  $text = ''
  if ($null -ne $output) {
    $text = [string]::Join("`n", $output)
  }
  return @{
    ExitCode = $LASTEXITCODE
    Text = $text
  }
}

function Invoke-BridgeCliJson {
  param([string[]]$CommandArgs)
  $text = Invoke-BridgeCli $CommandArgs
  return @{
    Text = $text
    Json = ($text | ConvertFrom-Json)
  }
}

function Wait-BridgeReady {
  param([int]$Attempts = 30, [int]$DelayMs = 250)
  for ($i = 0; $i -lt $Attempts; $i++) {
    Start-Sleep -Milliseconds $DelayMs
    $stdoutPath = Join-Path $script:BridgeRunDir 'wait-ready.stdout.log'
    $stderrPath = Join-Path $script:BridgeRunDir 'wait-ready.stderr.log'
    Remove-Item -Force $stdoutPath, $stderrPath -ErrorAction SilentlyContinue
    $probe = Start-Process -FilePath $script:BridgeCli `
      -ArgumentList ('ping "--workspace" "{0}" "--json"' -f ($script:BridgeWorkspace -replace '"', '\"')) `
      -Wait `
      -PassThru `
      -NoNewWindow `
      -RedirectStandardOutput $stdoutPath `
      -RedirectStandardError $stderrPath
    $ping = if (Test-Path $stdoutPath) { Get-Content $stdoutPath -Raw } else { '' }
    if ($probe.ExitCode -eq 0 -and $ping.Contains('"ok":true')) {
      $global:LASTEXITCODE = 0
      return
    }
    if ($i -eq ($Attempts - 1)) {
      $stderrText = if (Test-Path $stderrPath) { Get-Content $stderrPath -Raw } else { '' }
      $detail = ($ping + "`n" + $stderrText).Trim()
      throw "daemon did not become ready`n$detail"
    }
  }
  throw 'daemon did not become ready'
}

function Stop-BridgeDaemonProcesses {
  if ([string]::IsNullOrEmpty($script:BridgeDaemon)) {
    return
  }
  Get-CimInstance Win32_Process |
    Where-Object { $_.ExecutablePath -eq $script:BridgeDaemon } |
    ForEach-Object {
      try {
        Stop-Process -Id $_.ProcessId -Force -ErrorAction Stop
      } catch {
      }
    }
}

function Start-BridgeDaemon {
  param(
    [string]$Name = 'daemon',
    [string[]]$ExtraArgs = @()
  )
  $script:BridgeDaemonStdoutLog = Join-Path $script:BridgeRunDir "$Name.out.log"
  $script:BridgeDaemonStderrLog = Join-Path $script:BridgeRunDir "$Name.err.log"
  Remove-Item -Force $script:BridgeDaemonStdoutLog, $script:BridgeDaemonStderrLog -ErrorAction SilentlyContinue
  Stop-BridgeDaemonProcesses
  Write-BridgeLog "starting daemon for workspace: $script:BridgeWorkspace"
  $daemonArgs = @('--workspace', $script:BridgeWorkspace) + $ExtraArgs
  $daemonArgLine = [string]::Join(' ', ($daemonArgs | ForEach-Object { '"{0}"' -f ($_ -replace '"', '\"') }))
  $script:BridgeDaemonProc = Start-Process -FilePath $script:BridgeDaemon -ArgumentList $daemonArgLine -PassThru -RedirectStandardOutput $script:BridgeDaemonStdoutLog -RedirectStandardError $script:BridgeDaemonStderrLog
  Wait-BridgeReady
}

function Stop-BridgeDaemon {
  if ($script:BridgeDaemonProc -and -not $script:BridgeDaemonProc.HasExited) {
    Stop-Process -Id $script:BridgeDaemonProc.Id -Force
    $script:BridgeDaemonProc.WaitForExit()
  }
  $script:BridgeDaemonProc = $null
}

function Show-BridgeDiagnostics {
  Write-Host '--- daemon stdout ---'
  if ($script:BridgeDaemonStdoutLog -and (Test-Path $script:BridgeDaemonStdoutLog)) {
    Get-Content $script:BridgeDaemonStdoutLog -Raw | Write-Host
  }
  Write-Host '--- daemon stderr ---'
  if ($script:BridgeDaemonStderrLog -and (Test-Path $script:BridgeDaemonStderrLog)) {
    Get-Content $script:BridgeDaemonStderrLog -Raw | Write-Host
  }
  if ($script:BridgeRuntimeDir) {
    $runtimeLog = Join-Path $script:BridgeRuntimeDir 'runtime.log'
    if (Test-Path $runtimeLog) {
      Write-Host '--- runtime.log ---'
      Get-Content $runtimeLog -Raw | Write-Host
    }
  }
  if ($script:BridgeWorkspace) {
    $auditLog = Join-Path $script:BridgeWorkspace '.bridge\audit.log'
    if (Test-Path $auditLog) {
      Write-Host '--- audit.log ---'
      Get-Content $auditLog -Raw | Write-Host
    }
    $historyLog = Join-Path $script:BridgeWorkspace '.bridge\history.log'
    if (Test-Path $historyLog) {
      Write-Host '--- history.log ---'
      Get-Content $historyLog -Raw | Write-Host
    }
  }
}

function Set-BridgeRuntimeDirFromInfo {
  param($InfoObject)
  $script:BridgeRuntimeDir = [string](Get-JsonValue -InputObject $InfoObject -Path 'result.runtime_dir')
}

function Initialize-BridgeTest {
  param(
    [string]$Daemon,
    [string]$Cli,
    [string]$RunDir,
    [string]$Workspace
  )
  $script:BridgeDaemon = $Daemon
  $script:BridgeCli = $Cli
  $script:BridgeRunDir = $RunDir
  $script:BridgeWorkspace = $Workspace
  New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
}
