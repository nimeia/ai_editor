param(
  [string]$Daemon,
  [string]$Cli,
  [string]$RunDir
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'helpers/common.ps1')

$workspace = Join-Path $RunDir 'ws functional windows space'
$docsDir = Join-Path $workspace 'docs dir'
$relativePathWindows = 'docs dir\hello-world.md'
$newContentFile = Join-Path $RunDir 'new_content.txt'
$conflictNewContentFile = Join-Path $RunDir 'conflict_new.txt'
$crlfFile = Join-Path $workspace 'docs dir\crlf.txt'
$utf16File = Join-Path $workspace 'docs dir\utf16le.txt'
$binaryFile = Join-Path $workspace 'docs dir\binary.bin'
$excludedDir = Join-Path $workspace '.git'

Remove-Item -Recurse -Force $workspace -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $docsDir | Out-Null
New-Item -ItemType Directory -Force -Path $excludedDir | Out-Null
Set-Content -Path (Join-Path $docsDir 'hello-world.md') -Value "hello`nwindows bridge`n" -Encoding utf8
Set-Content -Path $newContentFile -Value "hello`npatched from windows`n" -Encoding utf8
[System.IO.File]::WriteAllText($crlfFile, "a`r`nb`r`n", [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllBytes($utf16File, [byte[]](0xFF,0xFE,0x68,0x00,0x69,0x00,0x0D,0x00,0x0A,0x00))
[System.IO.File]::WriteAllBytes($binaryFile, [byte[]](0x00,0x01,0x02,0x03,0xFF))
Set-Content -Path (Join-Path $excludedDir 'ignored.txt') -Value "hidden" -Encoding utf8

$env:AI_BRIDGE_LOG_ROTATE_BYTES = '120'
$env:AI_BRIDGE_LOG_ROTATE_KEEP = '2'
$env:AI_BRIDGE_HISTORY_ROTATE_BYTES = '80'
$env:AI_BRIDGE_HISTORY_ROTATE_KEEP = '2'
$env:AI_BRIDGE_BACKUP_KEEP = '2'
$env:AI_BRIDGE_PREVIEW_KEEP = '2'

Initialize-BridgeTest -Daemon $Daemon -Cli $Cli -RunDir $RunDir -Workspace $workspace
Start-BridgeDaemon -Name 'functional_windows_native'
try {
  $pingText = Invoke-BridgeCli @('ping', '--workspace', $workspace, '--json')
  Assert-Contains $pingText '"ok":true' 'ping should succeed'

  $infoResult = Invoke-BridgeCliJson @('info', '--workspace', $workspace, '--json')
  $infoText = $infoResult.Text
  $info = $infoResult.Json
  Set-BridgeRuntimeDirFromInfo -InfoObject $info
  Assert-Contains $infoText '"workspace_root":"' 'info should return workspace root'
  Assert-Contains $infoText '"endpoint":"\\\\.\\pipe\\aibridge-' 'info should return named pipe endpoint'
  Assert-JsonContains $info 'result.workspace_root' 'ws functional windows space' 'workspace root should preserve spaces'
  Assert-JsonContains $info 'result.runtime_dir' 'ai_bridge_runtime' 'runtime dir should be reported'
  Assert-JsonEq $info 'result.platform' 'windows' 'platform should be windows'
  Assert-JsonEq $info 'result.transport' 'windows-named-pipe' 'transport should be windows named pipe'

  $openResult = Invoke-BridgeCliJson @('open', '--workspace', $workspace, '--json')
  Assert-JsonTruthy $openResult.Json 'result.instance_key' 'open should return instance key'
  Assert-JsonContains $openResult.Json 'result.endpoint' '\\.\pipe\aibridge-' 'open should return named pipe endpoint'

  $resolveText = Invoke-BridgeCli @('resolve', '--workspace', $workspace, '--path', $relativePathWindows, '--json')
  Assert-Contains $resolveText '"relative_path":"docs dir/hello-world.md"' 'resolve should normalize backslash path to generic path'

  $listText = Invoke-BridgeCli @('list', '--workspace', $workspace, '--json', '--recursive')
  Assert-Contains $listText '"path":"docs dir/hello-world.md"' 'list should include spaced file path'
  Assert-NotContains $listText '"path":".git/ignored.txt"' 'list should skip excluded entries by default'

  $listExcludedText = Invoke-BridgeCli @('list', '--workspace', $workspace, '--json', '--recursive', '--include-excluded')
  Assert-Contains $listExcludedText '"path":".git/ignored.txt"' 'list include-excluded should show excluded entry'

  $statResult = Invoke-BridgeCliJson @('stat', '--workspace', $workspace, '--path', $relativePathWindows, '--json')
  Assert-JsonEq $statResult.Json 'result.kind' 'file' 'stat should report file kind'

  $crlfStat = (Invoke-BridgeCliJson @('stat', '--workspace', $workspace, '--path', 'docs dir/crlf.txt', '--json')).Json
  Assert-JsonEq $crlfStat 'result.eol' 'crlf' 'CRLF file should report eol=crlf'

  $utf16Stat = (Invoke-BridgeCliJson @('stat', '--workspace', $workspace, '--path', 'docs dir/utf16le.txt', '--json')).Json
  Assert-JsonEq $utf16Stat 'result.encoding' 'utf-16le' 'UTF-16LE file should be detected'
  Assert-JsonEq $utf16Stat 'result.bom' 'True' 'UTF-16LE BOM should be reported'

  $readText = Invoke-BridgeCli @('read', '--workspace', $workspace, '--path', $relativePathWindows, '--json')
  Assert-Contains $readText 'windows bridge' 'read should include file content'

  $binaryRead = Invoke-BridgeCliAllowFail @('read', '--workspace', $workspace, '--path', 'docs dir/binary.bin', '--json')
  Assert-True ($binaryRead.ExitCode -ne 0) 'binary read should fail'
  Assert-Contains $binaryRead.Text '"code":"BINARY_FILE"' 'binary read should return BINARY_FILE'

  $searchText = Invoke-BridgeCli @('search-text', '--workspace', $workspace, '--path', 'docs dir', '--query', 'windows', '--json')
  Assert-Contains $searchText '"ok":true' 'search should succeed'
  Assert-Contains $searchText '"path":"docs dir/hello-world.md"' 'search should match spaced path'

  $badRegex = Invoke-BridgeCliAllowFail @('search-regex', '--workspace', $workspace, '--path', 'docs dir', '--pattern', '(', '--json')
  Assert-True ($badRegex.ExitCode -ne 0) 'invalid regex should fail'
  Assert-Contains $badRegex.Text '"code":"INVALID_PARAMS"' 'invalid regex should report INVALID_PARAMS'

  $streamRead = Invoke-BridgeCli @('read', '--workspace', $workspace, '--path', $relativePathWindows, '--stream', '--chunk-bytes', '8', '--json')
  Assert-Contains $streamRead '"type":"chunk"' 'stream read should emit chunk frames'
  Assert-Contains $streamRead '"type":"final"' 'stream read should emit final frame'

  $previewText = Invoke-BridgeCli @('patch-preview', '--workspace', $workspace, '--path', $relativePathWindows, '--new-content-file', $newContentFile, '--json')
  Assert-Contains $previewText '"preview_id":"' 'patch preview should return preview_id'
  $preview = $previewText | ConvertFrom-Json
  $previewId = [string]$preview.result.preview_id
  Assert-True (-not [string]::IsNullOrEmpty($previewId)) 'preview id should not be empty'

  $applyText = Invoke-BridgeCli @('patch-apply', '--workspace', $workspace, '--preview-id', $previewId, '--json')
  Assert-Contains $applyText '"applied":true' 'patch apply should succeed'

  $historyText = Invoke-BridgeCli @('history', '--workspace', $workspace, '--path', $relativePathWindows, '--json')
  Assert-Contains $historyText 'patch.apply.preview' 'history should contain preview apply entry'

  Set-Content -Path (Join-Path $docsDir 'conflict.txt') -Value "old`n" -Encoding utf8
  Set-Content -Path $conflictNewContentFile -Value "patched`n" -Encoding utf8
  $conflictPreviewText = Invoke-BridgeCli @('patch-preview', '--workspace', $workspace, '--path', 'docs dir/conflict.txt', '--new-content-file', $conflictNewContentFile, '--json')
  $conflictPreview = $conflictPreviewText | ConvertFrom-Json
  $conflictPreviewId = [string]$conflictPreview.result.preview_id
  Set-Content -Path (Join-Path $docsDir 'conflict.txt') -Value "external change`n" -Encoding utf8
  $conflictApply = Invoke-BridgeCliAllowFail @('patch-apply', '--workspace', $workspace, '--preview-id', $conflictPreviewId, '--json')
  Assert-True ($conflictApply.ExitCode -ne 0) 'conflict apply should fail'
  Assert-Contains $conflictApply.Text '"code":"PATCH_CONFLICT"' 'conflict apply should report PATCH_CONFLICT'

  $runtimeLog = Join-Path $info.result.runtime_dir 'runtime.log'
  $auditLog = Join-Path $workspace '.bridge\audit.log'
  Assert-True (Test-Path $runtimeLog) "runtime log missing: $runtimeLog"
  Assert-True (Test-Path $auditLog) "audit log missing: $auditLog"
  Assert-Contains ([IO.File]::ReadAllText($runtimeLog)) 'method=patch.apply' 'runtime log should include patch.apply'
  Assert-Contains ([IO.File]::ReadAllText($auditLog)) 'patch.apply' 'audit log should include patch.apply'

  Write-BridgeLog 'functional_windows_native passed'
}
catch {
  Show-BridgeDiagnostics
  throw
}
finally {
  Stop-BridgeDaemon
}
