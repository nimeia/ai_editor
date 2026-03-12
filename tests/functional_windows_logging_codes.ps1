param(
  [string]$Daemon,
  [string]$Cli,
  [string]$RunDir
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'helpers/common.ps1')

function Get-RuntimeLogLines {
  param([string]$RuntimeDir)
  $pattern = Join-Path $RuntimeDir 'runtime.log*'
  $files = Get-ChildItem -Path $pattern -File -ErrorAction SilentlyContinue | Sort-Object Name
  $lines = @()
  foreach ($file in $files) {
    $content = Get-Content $file.FullName -ErrorAction SilentlyContinue
    if ($null -ne $content) {
      $lines += @($content)
    }
  }
  return $lines
}

function Get-AuditLogLines {
  param([string]$Workspace)
  $pattern = Join-Path $Workspace '.bridge\audit.log*'
  $files = Get-ChildItem -Path $pattern -File -ErrorAction SilentlyContinue | Sort-Object Name
  $lines = @()
  foreach ($file in $files) {
    $content = Get-Content $file.FullName -ErrorAction SilentlyContinue
    if ($null -ne $content) {
      $lines += @($content)
    }
  }
  return $lines
}

function Find-RuntimeLineByRequestId {
  param(
    [string]$RuntimeDir,
    [string]$RequestId
  )
  foreach ($line in (Get-RuntimeLogLines -RuntimeDir $RuntimeDir)) {
    if ($line -like "*request_id=$RequestId*") {
      return $line
    }
  }
  return $null
}

function Find-AuditRecordByRequestId {
  param(
    [string]$Workspace,
    [string]$RequestId
  )
  foreach ($line in (Get-AuditLogLines -Workspace $Workspace)) {
    $columns = $line -split "`t", 14
    if ($columns.Count -ge 14 -and $columns[1] -eq $RequestId) {
      return [pscustomobject]@{
        Timestamp     = $columns[0]
        RequestId     = $columns[1]
        ClientId      = $columns[2]
        SessionId     = $columns[3]
        Method        = $columns[4]
        Path          = $columns[5]
        Endpoint      = $columns[6]
        DurationMs    = $columns[7]
        RequestBytes  = $columns[8]
        ResponseBytes = $columns[9]
        Status        = $columns[10]
        Truncated     = $columns[11]
        ErrorCode     = $columns[12]
        ErrorMessage  = $columns[13]
        RawLine       = $line
      }
    }
  }
  return $null
}

function Wait-ForRuntimeLine {
  param(
    [string]$RuntimeDir,
    [string]$RequestId,
    [int]$Attempts = 30,
    [int]$DelayMs = 100
  )
  for ($i = 0; $i -lt $Attempts; $i++) {
    $line = Find-RuntimeLineByRequestId -RuntimeDir $RuntimeDir -RequestId $RequestId
    if ($null -ne $line) {
      return $line
    }
    Start-Sleep -Milliseconds $DelayMs
  }
  throw "runtime log line not found for request_id=$RequestId"
}

function Wait-ForAuditRecord {
  param(
    [string]$Workspace,
    [string]$RequestId,
    [int]$Attempts = 30,
    [int]$DelayMs = 100
  )
  for ($i = 0; $i -lt $Attempts; $i++) {
    $record = Find-AuditRecordByRequestId -Workspace $Workspace -RequestId $RequestId
    if ($null -ne $record) {
      return $record
    }
    Start-Sleep -Milliseconds $DelayMs
  }
  throw "audit log record not found for request_id=$RequestId"
}

function Assert-LoggedErrorConsistency {
  param(
    [string]$ResponseText,
    [string]$RequestId,
    [string]$Method,
    [string]$Path,
    [string]$ClientId,
    [string]$SessionId
  )

  $responseJson = $ResponseText | ConvertFrom-Json
  Assert-JsonEq $responseJson 'request_id' $RequestId 'response request_id mismatch'
  Assert-JsonEq $responseJson 'ok' 'False' 'response should be an error'
  $code = [string](Get-JsonValue -InputObject $responseJson -Path 'error.code')
  $message = [string](Get-JsonValue -InputObject $responseJson -Path 'error.message')
  Assert-True (-not [string]::IsNullOrEmpty($code)) 'error code should not be empty'
  Assert-True (-not [string]::IsNullOrEmpty($message)) 'error message should not be empty'

  $runtimeLine = Wait-ForRuntimeLine -RuntimeDir $script:BridgeRuntimeDir -RequestId $RequestId
  Assert-Contains $runtimeLine "`tWARN`t" "runtime log should mark failed request as WARN for $RequestId"
  Assert-Contains $runtimeLine "request_id=$RequestId" "runtime log should include request_id for $RequestId"
  Assert-Contains $runtimeLine "method=$Method" "runtime log should include method for $RequestId"
  Assert-Contains $runtimeLine 'ok=false' "runtime log should mark request as failed for $RequestId"
  Assert-Contains $runtimeLine "code=$code" "runtime log should include error code $code for $RequestId"
  if (-not [string]::IsNullOrEmpty($Path)) {
    Assert-Contains $runtimeLine "path=$Path" "runtime log should include path $Path for $RequestId"
  }

  $auditRecord = Wait-ForAuditRecord -Workspace $script:BridgeWorkspace -RequestId $RequestId
  Assert-True ($auditRecord.Method -eq $Method) "audit log method mismatch for $RequestId"
  Assert-True ($auditRecord.Path -eq $Path) "audit log path mismatch for $RequestId"
  Assert-True ($auditRecord.ClientId -eq $ClientId) "audit log client_id mismatch for $RequestId"
  Assert-True ($auditRecord.SessionId -eq $SessionId) "audit log session_id mismatch for $RequestId"
  Assert-True ($auditRecord.Status -eq 'error') "audit log status should be error for $RequestId"
  Assert-True ($auditRecord.ErrorCode -eq $code) "audit log error code mismatch for $RequestId"
  Assert-True ($auditRecord.ErrorMessage -eq $message) "audit log error message mismatch for $RequestId"
  Assert-True ($auditRecord.Truncated -eq 'false') "audit log truncated flag should be false for $RequestId"
}

$workspace = Join-Path $RunDir 'ws_windows_logging_codes'
$docsDir = Join-Path $workspace 'docs'
$newContentFile = Join-Path $RunDir 'logging_new_content.txt'

Remove-Item -Recurse -Force $workspace -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $docsDir | Out-Null
Set-Content -Path (Join-Path $docsDir 'sample.txt') -Value "hello`nwindows logging`n" -Encoding utf8
[System.IO.File]::WriteAllBytes((Join-Path $docsDir 'blob.bin'), [byte[]](0x00,0x01,0x02,0x03,0xFF))
Set-Content -Path $newContentFile -Value "hello`nwindows logging patched`n" -Encoding utf8

$env:AI_BRIDGE_LOG_ROTATE_BYTES = '4096'
$env:AI_BRIDGE_LOG_ROTATE_KEEP = '2'
$env:AI_BRIDGE_SEARCH_DELAY_MS = '2'

Initialize-BridgeTest -Daemon $Daemon -Cli $Cli -RunDir $RunDir -Workspace $workspace
Start-BridgeDaemon -Name 'functional_windows_logging_codes'
try {
  $info = Invoke-BridgeCliJson @('info', '--workspace', $workspace, '--json')
  Set-BridgeRuntimeDirFromInfo $info.Json

  $clientId = 'cli-win-log-001'
  $sessionId = 'sess-win-log-001'

  $readMissing = Invoke-BridgeCliAllowFail @('read', '--workspace', $workspace, '--path', 'docs/missing.txt', '--request-id', 'req-win-log-read-missing', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-True ($readMissing.ExitCode -eq 1) 'missing read should exit 1'
  Assert-LoggedErrorConsistency -ResponseText $readMissing.Text -RequestId 'req-win-log-read-missing' -Method 'fs.read' -Path 'docs/missing.txt' -ClientId $clientId -SessionId $sessionId

  $listFile = Invoke-BridgeCliAllowFail @('list', '--workspace', $workspace, '--path', 'docs/sample.txt', '--request-id', 'req-win-log-list-file', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-True ($listFile.ExitCode -eq 1) 'list on file should exit 1'
  Assert-LoggedErrorConsistency -ResponseText $listFile.Text -RequestId 'req-win-log-list-file' -Method 'fs.list' -Path 'docs/sample.txt' -ClientId $clientId -SessionId $sessionId

  $searchTimeout = Invoke-BridgeCliAllowFail @('search-text', '--workspace', $workspace, '--path', 'docs/sample.txt', '--query', 'missing-token', '--timeout-ms', '5', '--request-id', 'req-win-log-search-timeout', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-True ($searchTimeout.ExitCode -eq 1) 'search timeout should exit 1'
  Assert-LoggedErrorConsistency -ResponseText $searchTimeout.Text -RequestId 'req-win-log-search-timeout' -Method 'search.text' -Path 'docs/sample.txt' -ClientId $clientId -SessionId $sessionId

  $preview = Invoke-BridgeCliJson @('patch-preview', '--workspace', $workspace, '--path', 'docs/sample.txt', '--new-content-file', $newContentFile, '--request-id', 'req-win-log-preview', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  $previewId = [string](Get-JsonValue -InputObject $preview.Json -Path 'result.preview_id')
  Assert-True (-not [string]::IsNullOrEmpty($previewId)) 'patch preview should return preview_id'

  $previewMismatch = Invoke-BridgeCliAllowFail @('patch-apply', '--workspace', $workspace, '--path', 'docs/other.txt', '--preview-id', $previewId, '--request-id', 'req-win-log-preview-mismatch', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-True ($previewMismatch.ExitCode -eq 1) 'preview mismatch should exit 1'
  Assert-LoggedErrorConsistency -ResponseText $previewMismatch.Text -RequestId 'req-win-log-preview-mismatch' -Method 'patch.apply' -Path 'docs/other.txt' -ClientId $clientId -SessionId $sessionId

  $cancelEmpty = Invoke-BridgeCliAllowFail @('cancel', '--workspace', $workspace, '--target-request-id', '', '--request-id', 'req-win-log-cancel-empty', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-True ($cancelEmpty.ExitCode -eq 1) 'cancel without target should exit 1'
  Assert-LoggedErrorConsistency -ResponseText $cancelEmpty.Text -RequestId 'req-win-log-cancel-empty' -Method 'request.cancel' -Path '' -ClientId $clientId -SessionId $sessionId

  Write-BridgeLog 'functional_windows_logging_codes passed'
}
catch {
  Show-BridgeDiagnostics
  throw
}
finally {
  Stop-BridgeDaemon
}

exit 0
