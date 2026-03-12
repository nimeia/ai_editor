param(
  [string]$Daemon,
  [string]$Cli,
  [string]$RunDir
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'helpers/common.ps1')

$workspace = Join-Path $RunDir 'ws functional windows stream timeout'
$docsDir = Join-Path $workspace 'docs dir'
$bigFile = Join-Path $docsDir 'big.txt'
$newContentFile = Join-Path $RunDir 'new_big_content.txt'

Remove-Item -Recurse -Force $workspace -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $docsDir | Out-Null

$builder = [System.Text.StringBuilder]::new()
for ($i = 1; $i -le 2400; $i++) {
  [void]$builder.AppendFormat("line-{0:D4} token token token`n", $i)
}
[System.IO.File]::WriteAllText($bigFile, $builder.ToString(), [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($newContentFile, $builder.ToString().Replace('token', 'updated-token'), [System.Text.UTF8Encoding]::new($false))

$env:AI_BRIDGE_SEARCH_DELAY_MS = '2'
$env:AI_BRIDGE_READ_STREAM_DELAY_MS = '2'
$env:AI_BRIDGE_PATCH_STREAM_DELAY_MS = '2'

Initialize-BridgeTest -Daemon $Daemon -Cli $Cli -RunDir $RunDir -Workspace $workspace
Start-BridgeDaemon -Name 'functional_windows_stream_cancel_timeout'
try {
  $searchStreamText = Invoke-BridgeCli @('search-text', '--workspace', $workspace, '--path', 'docs dir/big.txt', '--query', 'token', '--stream', '--json')
  Assert-Contains $searchStreamText '"type":"chunk"' 'search stream should emit chunk frame'
  Assert-Contains $searchStreamText '"event":"search.match"' 'search stream should emit search.match'
  Assert-Contains $searchStreamText '"type":"final"' 'search stream should emit final frame'
  Assert-Contains $searchStreamText '"stream_event":"search.match"' 'search stream final summary should include stream_event'
  Assert-Contains $searchStreamText '"timed_out":false' 'search stream final summary should report timed_out=false'

  $readStreamText = Invoke-BridgeCli @('read', '--workspace', $workspace, '--path', 'docs dir/big.txt', '--max-bytes', '131072', '--chunk-bytes', '2048', '--stream', '--json')
  Assert-Contains $readStreamText '"event":"fs.read.chunk"' 'read stream should emit read chunks'
  Assert-Contains $readStreamText '"type":"final"' 'read stream should emit final frame'
  Assert-Contains $readStreamText '"stream_event":"fs.read.chunk"' 'read stream final summary should include stream_event'
  $readFrames = $readStreamText -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { $_ | ConvertFrom-Json }
  $readJoined = (($readFrames | Where-Object { $_.type -eq 'chunk' } | ForEach-Object { $_.data.content }) -join '')
  Assert-True ($readJoined.Contains('line-0001 token token token')) 'read stream should contain first line'
  Assert-True ($readJoined.Contains('line-2000 token token token')) 'read stream should contain later line'
  $readFinal = $readFrames | Where-Object { $_.type -eq 'final' } | Select-Object -Last 1
  Assert-True ($null -ne $readFinal) 'read stream should have final frame'
  Assert-True ([int]$readFinal.result.chunk_count -ge 2) 'read stream should report multiple chunks'

  $rangeStreamText = Invoke-BridgeCli @('read-range', '--workspace', $workspace, '--path', 'docs dir/big.txt', '--start', '10', '--end', '12', '--chunk-bytes', '8', '--stream', '--json')
  Assert-Contains $rangeStreamText '"event":"fs.read_range.chunk"' 'read-range stream should emit range chunks'
  Assert-Contains $rangeStreamText '"stream_event":"fs.read_range.chunk"' 'read-range final summary should include stream_event'
  $rangeFrames = $rangeStreamText -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { $_ | ConvertFrom-Json }
  $rangeJoined = (($rangeFrames | Where-Object { $_.type -eq 'chunk' } | ForEach-Object { $_.data.content }) -join '')
  Assert-True ($rangeJoined -eq "line-0010 token token token`nline-0011 token token token`nline-0012 token token token") 'read-range stream should reconstruct requested lines exactly'

  $patchStreamText = Invoke-BridgeCli @('patch-preview', '--workspace', $workspace, '--path', 'docs dir/big.txt', '--new-content-file', $newContentFile, '--chunk-bytes', '2048', '--stream', '--json')
  Assert-Contains $patchStreamText '"event":"patch.preview.chunk"' 'patch stream should emit preview chunks'
  Assert-Contains $patchStreamText '"type":"final"' 'patch stream should emit final frame'
  Assert-Contains $patchStreamText '"stream_event":"patch.preview.chunk"' 'patch stream final summary should include stream_event'
  $patchFrames = $patchStreamText -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { $_ | ConvertFrom-Json }
  $patchJoined = (($patchFrames | Where-Object { $_.type -eq 'chunk' } | ForEach-Object { $_.data.content }) -join '')
  Assert-True ($patchJoined.Contains('--- a/docs dir/big.txt')) 'patch stream should contain original diff header'
  Assert-True ($patchJoined.Contains('+++ b/docs dir/big.txt')) 'patch stream should contain new diff header'
  Assert-True ($patchJoined.Contains('+line-2000 updated-token updated-token updated-token')) 'patch stream should contain updated content'
  $patchFinal = $patchFrames | Where-Object { $_.type -eq 'final' } | Select-Object -Last 1
  Assert-True ($null -ne $patchFinal) 'patch stream should have final frame'
  Assert-True ([int]$patchFinal.result.chunk_count -ge 2) 'patch stream should report multiple chunks'

  $searchCancelOut = Join-Path $RunDir 'windows_search_cancel.out'
  $searchCancelErr = Join-Path $RunDir 'windows_search_cancel.err'
  Remove-Item -Force $searchCancelOut, $searchCancelErr -ErrorAction SilentlyContinue
  $searchProc = Start-Process -FilePath $Cli -ArgumentList @('search-text', '--workspace', $workspace, '--path', 'docs dir/big.txt', '--query', 'definitely-not-present', '--request-id', 'req-win-search-cancel', '--timeout-ms', '30000', '--json') -PassThru -RedirectStandardOutput $searchCancelOut -RedirectStandardError $searchCancelErr
  Start-Sleep -Milliseconds 200
  $searchCancelReply = Invoke-BridgeCli @('cancel', '--workspace', $workspace, '--target-request-id', 'req-win-search-cancel', '--request-id', 'req-win-search-cancel-sender', '--json')
  if (-not $searchProc.WaitForExit(10000)) {
    $searchProc | Stop-Process -Force
    throw 'search cancel subprocess did not exit in time'
  }
  $searchCombined = ''
  if (Test-Path $searchCancelOut) { $searchCombined += (Get-Content $searchCancelOut -Raw) }
  if (Test-Path $searchCancelErr) { $searchCombined += (Get-Content $searchCancelErr -Raw) }
  Assert-Contains $searchCancelReply '"ok":true' 'search cancel reply should succeed'
  Assert-Contains $searchCancelReply 'req-win-search-cancel' 'search cancel reply should echo target request id'
  Assert-Contains $searchCombined 'REQUEST_CANCELLED' 'cancelled search should report REQUEST_CANCELLED'

  $readCancelOut = Join-Path $RunDir 'windows_read_cancel.out'
  $readCancelErr = Join-Path $RunDir 'windows_read_cancel.err'
  Remove-Item -Force $readCancelOut, $readCancelErr -ErrorAction SilentlyContinue
  $readProc = Start-Process -FilePath $Cli -ArgumentList @('read', '--workspace', $workspace, '--path', 'docs dir/big.txt', '--request-id', 'req-win-read-cancel', '--stream', '--chunk-bytes', '64', '--timeout-ms', '30000', '--json') -PassThru -RedirectStandardOutput $readCancelOut -RedirectStandardError $readCancelErr
  Start-Sleep -Milliseconds 200
  $readCancelReply = Invoke-BridgeCli @('cancel', '--workspace', $workspace, '--target-request-id', 'req-win-read-cancel', '--request-id', 'req-win-read-cancel-sender', '--json')
  if (-not $readProc.WaitForExit(10000)) {
    $readProc | Stop-Process -Force
    throw 'read cancel subprocess did not exit in time'
  }
  $readCombined = ''
  if (Test-Path $readCancelOut) { $readCombined += (Get-Content $readCancelOut -Raw) }
  if (Test-Path $readCancelErr) { $readCombined += (Get-Content $readCancelErr -Raw) }
  Assert-Contains $readCancelReply '"ok":true' 'read cancel reply should succeed'
  Assert-Contains $readCombined 'REQUEST_CANCELLED' 'cancelled read stream should report REQUEST_CANCELLED'

  $searchTimeout = Invoke-BridgeCliAllowFail @('search-text', '--workspace', $workspace, '--path', 'docs dir/big.txt', '--query', 'definitely-not-present', '--request-id', 'req-win-timeout-search', '--timeout-ms', '5', '--json')
  Assert-True ($searchTimeout.ExitCode -ne 0) 'search timeout should fail'
  Assert-Contains $searchTimeout.Text '"code":"SEARCH_TIMEOUT"' 'search timeout should report SEARCH_TIMEOUT'

  $readTimeout = Invoke-BridgeCliAllowFail @('read', '--workspace', $workspace, '--path', 'docs dir/big.txt', '--request-id', 'req-win-timeout-read', '--stream', '--chunk-bytes', '64', '--timeout-ms', '5', '--json')
  Assert-True ($readTimeout.ExitCode -ne 0) 'read timeout should fail'
  Assert-Contains $readTimeout.Text '"code":"REQUEST_TIMEOUT"' 'read timeout should report REQUEST_TIMEOUT'

  $patchTimeout = Invoke-BridgeCliAllowFail @('patch-preview', '--workspace', $workspace, '--path', 'docs dir/big.txt', '--new-content-file', $newContentFile, '--request-id', 'req-win-timeout-patch', '--stream', '--chunk-bytes', '64', '--timeout-ms', '5', '--json')
  Assert-True ($patchTimeout.ExitCode -ne 0) 'patch preview timeout should fail'
  Assert-Contains $patchTimeout.Text '"code":"REQUEST_TIMEOUT"' 'patch timeout should report REQUEST_TIMEOUT'
}
catch {
  Show-BridgeDiagnostics
  throw
}
finally {
  Stop-BridgeDaemon
}

exit 0
