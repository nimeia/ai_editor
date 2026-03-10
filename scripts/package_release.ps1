param(
  [string]$BuildDir = "build",
  [string]$Config = "Release",
  [string]$OutDir = "dist",
  [string]$Generator = "ZIP",
  [int]$Jobs = 1,
  [switch]$RunTests
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64
cmake --build $BuildDir --config $Config --parallel $Jobs
if ($RunTests) {
  ctest --test-dir $BuildDir -C $Config --output-on-failure
}
cpack --config "$BuildDir/CPackConfig.cmake" -C $Config -G $Generator -B $OutDir

$hashFile = Join-Path $OutDir "SHA256SUMS.txt"
if (Test-Path $hashFile) {
  Remove-Item $hashFile -Force
}
Get-ChildItem -Path $OutDir -File | Where-Object { $_.Extension -in '.zip', '.gz', '.tgz' } | Sort-Object Name | ForEach-Object {
  $hash = (Get-FileHash -Path $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
  "$hash  $($_.Name)" | Out-File -FilePath $hashFile -Append -Encoding ascii
}

Write-Host "release artifacts:"
Get-ChildItem -Path $OutDir -File | Sort-Object Name | ForEach-Object { Write-Host $_.FullName }
