REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

SETLOCAL ENABLEEXTENSIONS

type ci\docfx.json > docfx.json
type ci\toc.yml > toc.yml
docfx build docfx.json > docfx.log
DEL docfx.json 2> NUL
DEL toc.yml 2> NUL
@IF NOT %ERRORLEVEL% == 0 (
  type docfx.log
  ECHO Error: docfx build failed. 1>&2
  EXIT /B %ERRORLEVEL%
)
@type docfx.log
@type docfx.log | findstr /C:"Build succeeded."
@IF NOT %ERRORLEVEL% == 0 (
  ECHO Error: you have introduced build warnings. 1>&2
  EXIT /B %ERRORLEVEL%
)
