param(
  [string]$Daemon,
  [string]$Cli,
  [string]$RunDir
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'helpers/common.ps1')

$workspace = Join-Path $RunDir 'ws functional windows cli contract'
$docsDir = Join-Path $workspace 'docs'
$sampleFile = Join-Path $docsDir 'sample.txt'
$otherFile = Join-Path $docsDir 'other.txt'
$binaryFile = Join-Path $docsDir 'blob.bin'
$newContentFile = Join-Path $RunDir 'sample_new.txt'

Remove-Item -Recurse -Force $workspace -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $docsDir | Out-Null
Set-Content -Path $sampleFile -Value "alpha`nbeta token`ngamma`n" -Encoding utf8
Set-Content -Path $otherFile -Value "other file`n" -Encoding utf8
[System.IO.File]::WriteAllBytes($binaryFile, [byte[]](0x00,0x01,0x02,0x74,0x6F,0x6B,0x65,0x6E,0x03))
Set-Content -Path $newContentFile -Value "alpha`nbeta updated-token`ngamma`n" -Encoding utf8

$env:AI_BRIDGE_SEARCH_DELAY_MS = '2'

Initialize-BridgeTest -Daemon $Daemon -Cli $Cli -RunDir $RunDir -Workspace $workspace
Start-BridgeDaemon -Name 'functional_windows_cli_contract'
try {
  $noArgs = Invoke-BridgeCliAllowFail -CommandArgs @()
  Assert-True ($noArgs.ExitCode -eq 1) 'bridge_cli with no args should exit 1'
  Assert-Contains $noArgs.Text 'usage: bridge_cli <ping|info|open|resolve|list|stat|read|read-range|search-text|search-regex|cancel|patch-preview|patch-apply|patch-rollback|history>' 'no-arg usage text should be shown'

  $unsupported = Invoke-BridgeCliAllowFail @('not-a-command')
  Assert-True ($unsupported.ExitCode -eq 1) 'unsupported command should exit 1'
  Assert-Contains $unsupported.Text 'unsupported command' 'unsupported command should explain failure'

  $versionText = Invoke-BridgeCli @('--version')
  Assert-Contains $versionText 'ai_bridge_cli ' 'version output should include binary name'
  Assert-Contains $versionText 'platform=' 'version output should include platform'
  Assert-Contains $versionText 'transport=' 'version output should include transport'

  $pingText = (Invoke-BridgeCli @('ping', '--workspace', $workspace)).Trim()
  Assert-True ($pingText -eq 'pong') 'ping human output should be pong'

  $listText = Invoke-BridgeCli @('list', '--workspace', $workspace, '--path', 'docs')
  Assert-Contains $listText 'docs/sample.txt [file] (normal)' 'list should show sample file in human mode'
  Assert-Contains $listText 'docs/other.txt [file] (normal)' 'list should show other file in human mode'

  $readText = Invoke-BridgeCli @('read', '--workspace', $workspace, '--path', 'docs/sample.txt')
  Assert-Contains $readText 'alpha' 'read human output should include first line'
  Assert-Contains $readText 'beta token' 'read human output should include token line'

  $searchText = Invoke-BridgeCli @('search-text', '--workspace', $workspace, '--path', 'docs/sample.txt', '--query', 'token')
  Assert-Contains $searchText 'docs/sample.txt:2' 'search human output should include file and line'
  Assert-Contains $searchText 'beta token' 'search human output should include snippet'

  $statText = Invoke-BridgeCli @('stat', '--workspace', $workspace, '--path', 'docs/sample.txt')
  Assert-Contains $statText 'path: docs/sample.txt' 'stat human output should include relative path'
  Assert-Contains $statText 'kind: file' 'stat human output should include kind'
  Assert-Contains $statText 'encoding: utf-8' 'stat human output should include encoding'

  $resolveText = Invoke-BridgeCli @('resolve', '--workspace', $workspace, '--path', 'docs/sample.txt')
  Assert-Contains $resolveText 'workspace_root: ' 'resolve human output should include workspace root'
  Assert-Contains $resolveText 'relative_path: docs/sample.txt' 'resolve human output should include relative path'
  Assert-Contains $resolveText 'policy: normal' 'resolve human output should include policy'

  $previewText = Invoke-BridgeCli @('patch-preview', '--workspace', $workspace, '--path', 'docs/sample.txt', '--new-content-file', $newContentFile)
  Assert-Contains $previewText 'preview_id: ' 'patch-preview human output should include preview id'
  Assert-Contains $previewText '--- a/docs/sample.txt' 'patch-preview human output should include original diff header'
  Assert-Contains $previewText '+++ b/docs/sample.txt' 'patch-preview human output should include new diff header'

  $readMissing = Invoke-BridgeCliAllowFail @('read', '--workspace', $workspace, '--path', 'docs/missing.txt')
  Assert-True ($readMissing.ExitCode -eq 1) 'missing read should exit 1'
  Assert-Contains $readMissing.Text 'error [FILE_NOT_FOUND]: path not found' 'missing read should report FILE_NOT_FOUND in human mode'

  $readBinary = Invoke-BridgeCliAllowFail @('read', '--workspace', $workspace, '--path', 'docs/blob.bin')
  Assert-True ($readBinary.ExitCode -eq 1) 'binary read should exit 1'
  Assert-Contains $readBinary.Text 'error [BINARY_FILE]: binary file' 'binary read should report BINARY_FILE in human mode'

  $badRegex = Invoke-BridgeCliAllowFail @('search-regex', '--workspace', $workspace, '--path', 'docs/sample.txt', '--pattern', '[')
  Assert-True ($badRegex.ExitCode -eq 1) 'invalid regex should exit 1'
  Assert-Contains $badRegex.Text 'error [INVALID_PARAMS]:' 'invalid regex should report INVALID_PARAMS in human mode'

  $missingContent = Invoke-BridgeCliAllowFail @('patch-preview', '--workspace', $workspace, '--path', 'docs/sample.txt', '--new-content-file', (Join-Path $RunDir 'does-not-exist.txt'))
  Assert-True ($missingContent.ExitCode -eq 1) 'missing new content file should exit 1'
  Assert-Contains $missingContent.Text 'failed to open file:' 'missing new content file should fail before request dispatch'

  $resolveOutside = Invoke-BridgeCliAllowFail @('resolve', '--workspace', $workspace, '--path', '..\outside.txt', '--json')
  Assert-True ($resolveOutside.ExitCode -eq 1) 'outside resolve should exit 1'
  Assert-Contains $resolveOutside.Text '"code":"PATH_OUTSIDE_WORKSPACE"' 'outside resolve should report PATH_OUTSIDE_WORKSPACE'
  Assert-Contains $resolveOutside.Text '"message":"path is outside workspace"' 'outside resolve should report outside-workspace message'

  $listFile = Invoke-BridgeCliAllowFail @('list', '--workspace', $workspace, '--path', 'docs/sample.txt', '--json')
  Assert-True ($listFile.ExitCode -eq 1) 'list on file should exit 1'
  Assert-Contains $listFile.Text '"code":"INVALID_PARAMS"' 'list on file should report INVALID_PARAMS'
  Assert-Contains $listFile.Text '"message":"path is not a directory"' 'list on file should report directory message'

  $readMissingJson = Invoke-BridgeCliAllowFail @('read', '--workspace', $workspace, '--path', 'docs/missing.txt', '--json')
  Assert-True ($readMissingJson.ExitCode -eq 1) 'json missing read should exit 1'
  Assert-Contains $readMissingJson.Text '"code":"FILE_NOT_FOUND"' 'json missing read should report FILE_NOT_FOUND'

  $readBinaryJson = Invoke-BridgeCliAllowFail @('read', '--workspace', $workspace, '--path', 'docs/blob.bin', '--json')
  Assert-True ($readBinaryJson.ExitCode -eq 1) 'json binary read should exit 1'
  Assert-Contains $readBinaryJson.Text '"code":"BINARY_FILE"' 'json binary read should report BINARY_FILE'

  $badRegexJson = Invoke-BridgeCliAllowFail @('search-regex', '--workspace', $workspace, '--path', 'docs/sample.txt', '--pattern', '[', '--json')
  Assert-True ($badRegexJson.ExitCode -eq 1) 'json invalid regex should exit 1'
  Assert-Contains $badRegexJson.Text '"code":"INVALID_PARAMS"' 'json invalid regex should report INVALID_PARAMS'

  $searchTimeout = Invoke-BridgeCliAllowFail @('search-text', '--workspace', $workspace, '--path', 'docs/sample.txt', '--query', 'missing-token', '--timeout-ms', '5', '--json')
  Assert-True ($searchTimeout.ExitCode -eq 1) 'search timeout should exit 1'
  Assert-Contains $searchTimeout.Text '"code":"SEARCH_TIMEOUT"' 'search timeout should report SEARCH_TIMEOUT'

  $previewJsonText = Invoke-BridgeCli @('patch-preview', '--workspace', $workspace, '--path', 'docs/sample.txt', '--new-content-file', $newContentFile, '--request-id', 'req-win-cli-preview', '--json')
  Assert-Contains $previewJsonText '"ok":true' 'json patch preview should succeed'
  $previewJson = $previewJsonText | ConvertFrom-Json
  $previewId = [string]$previewJson.result.preview_id
  Assert-True (-not [string]::IsNullOrEmpty($previewId)) 'json patch preview should return preview_id'

  $previewNotFound = Invoke-BridgeCliAllowFail @('patch-apply', '--workspace', $workspace, '--path', 'docs/sample.txt', '--preview-id', 'preview-missing', '--json')
  Assert-True ($previewNotFound.ExitCode -eq 1) 'missing preview apply should exit 1'
  Assert-Contains $previewNotFound.Text '"code":"PREVIEW_NOT_FOUND"' 'missing preview apply should report PREVIEW_NOT_FOUND'

  $previewMismatch = Invoke-BridgeCliAllowFail @('patch-apply', '--workspace', $workspace, '--path', 'docs/other.txt', '--preview-id', $previewId, '--json')
  Assert-True ($previewMismatch.ExitCode -eq 1) 'preview mismatch should exit 1'
  Assert-Contains $previewMismatch.Text '"code":"PREVIEW_MISMATCH"' 'preview mismatch should report PREVIEW_MISMATCH'
  Assert-Contains $previewMismatch.Text '"message":"preview path mismatch"' 'preview mismatch should report path mismatch message'

  $rollbackMissing = Invoke-BridgeCliAllowFail @('patch-rollback', '--workspace', $workspace, '--path', 'docs/sample.txt', '--backup-id', 'backup-missing', '--json')
  Assert-True ($rollbackMissing.ExitCode -eq 1) 'missing rollback should exit 1'
  Assert-Contains $rollbackMissing.Text '"code":"ROLLBACK_NOT_FOUND"' 'missing rollback should report ROLLBACK_NOT_FOUND'
  Assert-Contains $rollbackMissing.Text '"message":"backup not found"' 'missing rollback should report backup-not-found message'

  Write-BridgeLog 'functional_windows_cli_contract passed'
}
catch {
  Show-BridgeDiagnostics
  throw
}
finally {
  Stop-BridgeDaemon
}

exit 0
