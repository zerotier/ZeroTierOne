@echo off
rem TAP-Windows -- A kernel driver to provide virtual tap
rem                 device functionality on Windows.
rem
rem  Copyright (C) 2012      Alon Bar-Lev <alon.barlev@gmail.com>
rem
rem  This program is free software; you can redistribute it and/or modify
rem  it under the terms of the GNU General Public License as published by
rem  the Free Software Foundation; either version 2 of the License, or
rem  (at your option) any later version.
rem
rem  This program is distributed in the hope that it will be useful,
rem  but WITHOUT ANY WARRANTY; without even the implied warranty of
rem  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
rem  GNU General Public License for more details.
rem
rem  You should have received a copy of the GNU General Public License
rem  along with this program (see the file COPYING included with this
rem  distribution); if not, write to the Free Software Foundation, Inc.,
rem  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

setlocal

set wd=%cd%
cd %0\..

if "%MAKENSIS%"=="" call ..\config-env.bat

if "%OUTDIR%"=="" set OUTDIR=.
set OUTPUT=%OUTDIR%\tap-windows-%PRODUCT_VERSION%
set TAP_ROOT=tmp\image\tap-windows-%PRODUCT_VERSION%

set SIGNTOOL_CMD="%SIGNTOOL%" sign /v /p "%CODESIGN_PASS%" /f "%CODESIGN_PKCS12%"
set SIGNTOOL_CMD_DRIVERS=%SIGNTOOL_CMD%
if "%CODESIGN_ISTEST%" NEQ "yes" (
	set SIGNTOOL_CMD=%SIGNTOOL_CMD% /t "%CODESIGN_TIMESTAMP%"
	set SIGNTOOL_CMD_DRIVERS=%SIGNTOOL_CMD% /ac "%CODESIGN_CROSS%"
)

del "%OUTPUT%.*" > nul 2>&1
rmdir /q /s tmp > nul 2>&1

mkdir %TAP_ROOT%\include
copy ..\src\tap-windows.h %TAP_ROOT%\include
if errorlevel 1 goto error
mkdir %TAP_ROOT%\i386
copy ..\src\i386\*  %TAP_ROOT%\i386
if errorlevel 1 goto error
mkdir %TAP_ROOT%\amd64
copy ..\src\amd64\* %TAP_ROOT%\amd64
if errorlevel 1 goto error

type ..\COPYING > %TAP_ROOT%\license.txt
type ..\COPYRIGHT.GPL >> %TAP_ROOT%\license.txt
cscript //nologo ..\build\unix2dos.js %TAP_ROOT%\license.txt
if errorlevel 1 goto error

"%DDK%\bin\selfsign\inf2cat" /driver:%TAP_ROOT%\i386 /os:XP_X86,Vista_X86,7_X86,Server2003_X86,Server2008_X86
"%DDK%\bin\selfsign\inf2cat" /driver:%TAP_ROOT%\amd64 /os:XP_X64,Vista_X64,7_X64,Server2003_X64,Server2008_X64,Server2008R2_X64

if not "%CODESIGN_PKCS12%"=="" (
	for %%a in (i386 amd64) do (
		%SIGNTOOL_CMD_DRIVERS% "%TAP_ROOT%\%%a\%PRODUCT_TAP_WIN_COMPONENT_ID%.sys"
		if errorlevel 1 goto error
		%SIGNTOOL_CMD_DRIVERS% "%TAP_ROOT%\%%a\%PRODUCT_TAP_WIN_COMPONENT_ID%.cat"
		if errorlevel 1 goto error
	)
)

cscript //nologo ..\build\zip.js tmp\image "%OUTPUT%.zip"
if errorlevel 1 goto error

"%MAKENSIS%\makensis" -DDEVCON32="%DEVCON32%" -DDEVCON64="%DEVCON64%" -DDEVCON_BASENAME="%DEVCON_BASENAME%" -DPRODUCT_TAP_WIN_COMPONENT_ID="%PRODUCT_TAP_WIN_COMPONENT_ID%" -DPRODUCT_NAME="%PRODUCT_NAME%" -DPRODUCT_VERSION="%PRODUCT_VERSION%" -DOUTPUT="%OUTPUT%.exe" -DIMAGE="%TAP_ROOT%" tap-windows.nsi
if errorlevel 1 goto error

if not "%CODESIGN_PKCS12%"=="" (
	%SIGNTOOL_CMD% "%OUTPUT%.exe"
	if errorlevel 1 goto error
)

set rc=0
goto end

:error
echo FATAL
set rc=1
goto end

:end

cd %wd%

endlocal

exit /b %rc%
