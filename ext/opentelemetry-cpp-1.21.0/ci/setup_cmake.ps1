# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

$ErrorActionPreference = "Stop"
trap { $host.SetShouldExit(1) }

if (-not $env:CMAKE_VERSION) { $env:CMAKE_VERSION = "3.31.6" }
$CMAKE_VERSION = $env:CMAKE_VERSION

choco uninstall cmake cmake.install -y --remove-dependencies --skip-autouninstaller --force --no-progress

Write-Host "Installing CMake version $CMAKE_VERSION ..."
choco install cmake --version=$CMAKE_VERSION --allow-downgrade -y --force --no-progress

function Get-Version {
    param (
        [string]$output
    )
    if ($output -match '(\d+\.\d+\.\d+)') {
        return $matches[1]
    }
    return $null
}

$cmakeOutput = & cmake --version | Select-Object -First 1
$ctestOutput = & ctest --version | Select-Object -First 1
$cpackOutput = & cpack --version | Select-Object -First 1

$cmakeVersion = Get-Version $cmakeOutput
$ctestVersion = Get-Version $ctestOutput
$cpackVersion = Get-Version $cpackOutput

Write-Host "cmake version $cmakeVersion detected"
Write-Host "ctest version $ctestVersion detected"
Write-Host "cpack version $cpackVersion detected"

if ($cmakeVersion -ne $CMAKE_VERSION) {
    Write-Error "CMake version mismatch: expected $CMAKE_VERSION, installed $cmakeVersion"
    exit 1
}
if ($ctestVersion -ne $CMAKE_VERSION) {
    Write-Error "CTest version mismatch: expected $CMAKE_VERSION, installed $ctestVersion"
    exit 1
}
if ($cpackVersion -ne $CMAKE_VERSION) {
    Write-Error "CPack version mismatch: expected $CMAKE_VERSION, installed $cpackVersion"
    exit 1
}
