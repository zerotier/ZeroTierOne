# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

$llvmVersion = "10.0.0"
Write-Host "Installing LLVM $llvmVersion ..." -ForegroundColor Cyan
Write-Host "Downloading..."
$exePath = "$env:temp\LLVM-$llvmVersion-win32.exe"
(New-Object Net.WebClient).DownloadFile("https://github.com/llvm/llvm-project/releases/download/llvmorg-$llvmVersion/LLVM-$llvmVersion-win32.exe", $exePath)
Write-Host "Installing..."
cmd /c start /wait $exePath /S
Write-Host "Installed" -ForegroundColor Green
