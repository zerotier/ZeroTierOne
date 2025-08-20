REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

@echo off
setlocal enabledelayedexpansion
if "%1" == "format" (
  if NOT "%2" == "" (
    where clang-format > NUL
    if %ERRORLEVEL% neq 0 (
      echo clang-format.exe not found in PATH!
      echo Assuming default path for LLVM tools...
      set PATH="C:\Program Files\LLVM\bin;!PATH!"
    )
    REM Assume if file exists - it's a full path, else - it's a path relative to git root.
    if exist %2 (
      set "FILEPATH=%2"
    ) else (
      set "FILEPATH=%GIT_PREFIX%%2"
    )
    clang-format -style=file -i --verbose !FILEPATH!
  )
)
endlocal
