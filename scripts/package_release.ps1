param(
  [string]$BuildDir = "build",
  [string]$Config = "Release",
  [string]$OutDir = "dist",
  [string]$Generator = "",
  [string[]]$Generators = @(),
  [int]$Jobs = 1,
  [switch]$RunTests
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

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

function Get-DefaultPackageGenerators {
  if ($IsWindows -or $env:OS -eq 'Windows_NT') {
    return @('ZIP', 'NSIS')
  }

  if ($IsMacOS) {
    return @('TGZ', 'productbuild')
  }

  return @('TGZ', 'DEB')
}

$resolvedGenerators = @()
if ($Generators.Count -gt 0) {
  $resolvedGenerators += $Generators
}
if ($Generator) {
  $resolvedGenerators += ($Generator -split ',')
}
$resolvedGenerators = @($resolvedGenerators | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($resolvedGenerators.Count -eq 0) {
  $resolvedGenerators = Get-DefaultPackageGenerators
}

$cmakeGenerator = Get-CmakeGenerator -BuildDir $BuildDir
if (-not $cmakeGenerator) {
  cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64
}
cmake --build $BuildDir --config $Config --parallel $Jobs
if ($RunTests) {
  ctest --test-dir $BuildDir -C $Config --output-on-failure
}
foreach ($current in $resolvedGenerators) {
  Write-Host "Packaging with generator: $current"
  cpack --config "$BuildDir/CPackConfig.cmake" -C $Config -G $current -B $OutDir
}

$hashFile = Join-Path $OutDir "SHA256SUMS.txt"
if (Test-Path $hashFile) {
  Remove-Item $hashFile -Force
}
Get-ChildItem -Path $OutDir -File | Where-Object { $_.Extension -in '.zip', '.gz', '.tgz', '.deb', '.pkg', '.dmg', '.exe', '.msi' } | Sort-Object Name | ForEach-Object {
  $hash = (Get-FileHash -Path $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
  "$hash  $($_.Name)" | Out-File -FilePath $hashFile -Append -Encoding ascii
}

Write-Host "release artifacts:"
Get-ChildItem -Path $OutDir -File | Sort-Object Name | ForEach-Object { Write-Host $_.FullName }
