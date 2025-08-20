REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

@REM This script allows to download a file to local machine. First argument is URL
set "PATH=%SystemRoot%;%SystemRoot%\System32;%SystemRoot%\System32\WindowsPowerShell\v1.0\;%ProgramFiles%\Git\bin"
@powershell -File Download.ps1 %1
