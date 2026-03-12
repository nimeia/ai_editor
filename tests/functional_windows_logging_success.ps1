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

function Assert-NonNegativeIntegerString {
  param(
    [string]$Value,
    [string]$Label
  )
  Assert-True ($Value -match '^[0-9]+$') "$Label should be a non-negative integer string"
}

function Assert-LoggedSuccessConsistency {
  param(
    [string]$ResponseText,
    [string]$RequestId,
    [string]$Method,
    [string]$Path,
    [string]$ClientId,
    [string]$SessionId,
    [bool]$ExpectedTruncated = $false,
    [string]$ExpectedJsonPath = ''
  )

  $responseJson = $ResponseText | ConvertFrom-Json
  Assert-JsonEq $responseJson 'request_id' $RequestId 'response request_id mismatch'
  Assert-JsonEq $responseJson 'ok' 'True' 'response should be successful'
  if (-not [string]::IsNullOrEmpty($ExpectedJsonPath)) {
    Assert-JsonTruthy $responseJson $ExpectedJsonPath "response should include $ExpectedJsonPath"
  }

  $runtimeLine = Wait-ForRuntimeLine -RuntimeDir $script:BridgeRuntimeDir -RequestId $RequestId
  Assert-Contains $runtimeLine "`tINFO`t" "runtime log should mark successful request as INFO for $RequestId"
  Assert-Contains $runtimeLine "request_id=$RequestId" "runtime log should include request_id for $RequestId"
  Assert-Contains $runtimeLine "method=$Method" "runtime log should include method for $RequestId"
  Assert-Contains $runtimeLine 'ok=true' "runtime log should mark request as successful for $RequestId"
  Assert-Contains $runtimeLine "truncated=$($ExpectedTruncated.ToString().ToLowerInvariant())" "runtime log truncated mismatch for $RequestId"
  Assert-NotContains $runtimeLine ' code=' "runtime log should not include an error code for successful request $RequestId"
  if (-not [string]::IsNullOrEmpty($Path)) {
    Assert-Contains $runtimeLine "path=$Path" "runtime log should include path $Path for $RequestId"
  }

  $auditRecord = Wait-ForAuditRecord -Workspace $script:BridgeWorkspace -RequestId $RequestId
  Assert-True ($auditRecord.Method -eq $Method) "audit log method mismatch for $RequestId"
  Assert-True ($auditRecord.Path -eq $Path) "audit log path mismatch for $RequestId"
  Assert-True ($auditRecord.ClientId -eq $ClientId) "audit log client_id mismatch for $RequestId"
  Assert-True ($auditRecord.SessionId -eq $SessionId) "audit log session_id mismatch for $RequestId"
  Assert-True ($auditRecord.Status -eq 'ok') "audit log status should be ok for $RequestId"
  Assert-True ($auditRecord.Truncated -eq $ExpectedTruncated.ToString().ToLowerInvariant()) "audit log truncated flag mismatch for $RequestId"
  Assert-True ([string]::IsNullOrEmpty($auditRecord.ErrorCode)) "audit log error code should be empty for successful request $RequestId"
  Assert-True ([string]::IsNullOrEmpty($auditRecord.ErrorMessage)) "audit log error message should be empty for successful request $RequestId"
  Assert-True (-not [string]::IsNullOrEmpty($auditRecord.Endpoint)) "audit log endpoint should not be empty for $RequestId"
  Assert-NonNegativeIntegerString -Value $auditRecord.DurationMs -Label "audit duration_ms"
  Assert-NonNegativeIntegerString -Value $auditRecord.RequestBytes -Label "audit request_bytes"
  Assert-NonNegativeIntegerString -Value $auditRecord.ResponseBytes -Label "audit response_bytes"
  Assert-True ([int64]$auditRecord.RequestBytes -gt 0) "audit request_bytes should be > 0 for $RequestId"
  Assert-True ([int64]$auditRecord.ResponseBytes -eq $ResponseText.Length) "audit response_bytes should match emitted json length for $RequestId"
  Assert-True ($auditRecord.Timestamp -match '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$') "audit timestamp format mismatch for $RequestId"
}

$workspace = Join-Path $RunDir 'ws_windows_logging_success'
$docsDir = Join-Path $workspace 'docs'
$newContentFile = Join-Path $RunDir 'logging_success_new_content.txt'

Remove-Item -Recurse -Force $workspace -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $docsDir | Out-Null
Set-Content -Path (Join-Path $docsDir 'sample.txt') -Value "hello`nwindows logging success`npatch me`n" -Encoding utf8
Set-Content -Path (Join-Path $docsDir 'large.txt') -Value ('0123456789abcdef' * 32) -Encoding utf8
Set-Content -Path $newContentFile -Value "hello`nwindows logging success`npatched`n" -Encoding utf8

$env:AI_BRIDGE_LOG_ROTATE_BYTES = '4096'
$env:AI_BRIDGE_LOG_ROTATE_KEEP = '2'
$env:AI_BRIDGE_SEARCH_DELAY_MS = '1'

Initialize-BridgeTest -Daemon $Daemon -Cli $Cli -RunDir $RunDir -Workspace $workspace
Start-BridgeDaemon -Name 'functional_windows_logging_success'
try {
  $info = Invoke-BridgeCliJson @('info', '--workspace', $workspace, '--json')
  Set-BridgeRuntimeDirFromInfo $info.Json

  $clientId = 'cli-win-log-ok-001'
  $sessionId = 'sess-win-log-ok-001'

  $stat = Invoke-BridgeCliJson @('stat', '--workspace', $workspace, '--path', 'docs/sample.txt', '--request-id', 'req-win-log-ok-stat', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-LoggedSuccessConsistency -ResponseText $stat.Text -RequestId 'req-win-log-ok-stat' -Method 'fs.stat' -Path 'docs/sample.txt' -ClientId $clientId -SessionId $sessionId

  $list = Invoke-BridgeCliJson @('list', '--workspace', $workspace, '--path', 'docs', '--request-id', 'req-win-log-ok-list', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-JsonLenGe $list.Json 'result.entries' 2 'list should include docs entries'
  Assert-LoggedSuccessConsistency -ResponseText $list.Text -RequestId 'req-win-log-ok-list' -Method 'fs.list' -Path 'docs' -ClientId $clientId -SessionId $sessionId

  $read = Invoke-BridgeCliJson @('read', '--workspace', $workspace, '--path', 'docs/sample.txt', '--request-id', 'req-win-log-ok-read', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-JsonContains $read.Json 'result.content' 'windows logging success' 'read should include file content'
  Assert-LoggedSuccessConsistency -ResponseText $read.Text -RequestId 'req-win-log-ok-read' -Method 'fs.read' -Path 'docs/sample.txt' -ClientId $clientId -SessionId $sessionId

  $readTruncated = Invoke-BridgeCliJson @('read', '--workspace', $workspace, '--path', 'docs/large.txt', '--max-bytes', '16', '--request-id', 'req-win-log-ok-read-truncated', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-JsonEq $readTruncated.Json 'result.truncated' 'True' 'truncated read should set result.truncated'
  Assert-LoggedSuccessConsistency -ResponseText $readTruncated.Text -RequestId 'req-win-log-ok-read-truncated' -Method 'fs.read' -Path 'docs/large.txt' -ClientId $clientId -SessionId $sessionId -ExpectedTruncated $true

  $search = Invoke-BridgeCliJson @('search-text', '--workspace', $workspace, '--path', 'docs/sample.txt', '--query', 'patch', '--request-id', 'req-win-log-ok-search', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-JsonLenGe $search.Json 'result.matches' 1 'search should produce at least one match'
  Assert-LoggedSuccessConsistency -ResponseText $search.Text -RequestId 'req-win-log-ok-search' -Method 'search.text' -Path 'docs/sample.txt' -ClientId $clientId -SessionId $sessionId

  $preview = Invoke-BridgeCliJson @('patch-preview', '--workspace', $workspace, '--path', 'docs/sample.txt', '--new-content-file', $newContentFile, '--request-id', 'req-win-log-ok-preview', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  $previewId = [string](Get-JsonValue -InputObject $preview.Json -Path 'result.preview_id')
  Assert-True (-not [string]::IsNullOrEmpty($previewId)) 'patch preview should return preview_id'
  Assert-LoggedSuccessConsistency -ResponseText $preview.Text -RequestId 'req-win-log-ok-preview' -Method 'patch.preview' -Path 'docs/sample.txt' -ClientId $clientId -SessionId $sessionId -ExpectedJsonPath 'result.preview_id'

  $apply = Invoke-BridgeCliJson @('patch-apply', '--workspace', $workspace, '--path', 'docs/sample.txt', '--preview-id', $previewId, '--request-id', 'req-win-log-ok-apply', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  $backupId = [string](Get-JsonValue -InputObject $apply.Json -Path 'result.backup_id')
  Assert-True (-not [string]::IsNullOrEmpty($backupId)) 'patch apply should return backup_id'
  Assert-LoggedSuccessConsistency -ResponseText $apply.Text -RequestId 'req-win-log-ok-apply' -Method 'patch.apply' -Path 'docs/sample.txt' -ClientId $clientId -SessionId $sessionId -ExpectedJsonPath 'result.backup_id'

  $rollback = Invoke-BridgeCliJson @('patch-rollback', '--workspace', $workspace, '--path', 'docs/sample.txt', '--backup-id', $backupId, '--request-id', 'req-win-log-ok-rollback', '--client-id', $clientId, '--session-id', $sessionId, '--json')
  Assert-JsonTruthy $rollback.Json 'result.restored' 'patch rollback should restore the original file'
  Assert-LoggedSuccessConsistency -ResponseText $rollback.Text -RequestId 'req-win-log-ok-rollback' -Method 'patch.rollback' -Path 'docs/sample.txt' -ClientId $clientId -SessionId $sessionId -ExpectedJsonPath 'result.restored'

  Write-BridgeLog 'functional_windows_logging_success passed'
}
catch {
  Show-BridgeDiagnostics
  throw
}
finally {
  Stop-BridgeDaemon
}

exit 0
