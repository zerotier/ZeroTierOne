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

cd /d %0\..

if "%1"=="--help" (
	echo %0
	echo Environment:
	echo     DDK                   DDK home
	echo     SIGNTOOL              signtool, default from DDK
	echo     DEVCON32              devcon, default from DDK
	echo     DEVCON64              devcon, default from DDK
	echo     MAKENSIS              nullsoft installer
	echo     CODESIGN_PKCS12       Code sign PKCS#12 optional
	echo     CODESIGN_PASS         Code sign password
	echo     CODESIGN_CROSS        Cross certificate to be used
	echo     CODESIGN_TIMESTAMP    Timestamp URL
	echo     CODESIGN_ISTEST       If yes, use test certificate
	echo     OUTDIR                Output directory
	exit /b 1
)

setlocal

if "%DDK%"=="" for /d %%f in (c:\WINDDK\*) do set DDK=%%f

if "%DDK%"=="" (
	echo cannot find ddk
	goto error
)

if "%SIGNTOOL%"=="" set SIGNTOOL=%DDK%\bin\x86\signtool.exe
if "%DEVCON32%"=="" set DEVCON32=%DDK%\tools\devcon\i386\devcon.exe
if "%DEVCON64%"=="" set DEVCON64=%DDK%\tools\devcon\amd64\devcon.exe
for /f %%f in ("%DEVCON32%") do set DEVCON_BASENAME=%%~nf%%~xf

if "%MAKENSIS%"=="" for /d %%f in ("%ProgramFiles%\NSIS" "%ProgramFiles(x86)%\NSIS") do if exist "%%f" set MAKENSIS=%%~f

if "%MAKENSIS%"=="" (
	echo cannot find nsis
	goto error
)

if "%CODESIGN_CROSS%"=="" set CODESIGN_CROSS=%cd%\build\MSCV-VSClass3.cer
if "%CODESIGN_TIMESTAMP%"=="" set CODESIGN_TIMESTAMP=http://timestamp.verisign.com/scripts/timestamp.dll

if "%OUTDIR%"=="" set OUTDIR=%cd%

set msvcg_args=cscript //nologo build/msvc-generate.js --config=version.m4
if exist config-local.m4 set msvcg_args=%msvcg_args% --config=config-local.m4
set msvcg_args=%msvcg_args% --var=DDK="%DDK%" --var=MAKENSIS="%MAKENSIS%" --var=SIGNTOOL="%SIGNTOOL%" --var=DEVCON32="%DEVCON32%" --var=DEVCON64="%DEVCON64%" --var=DEVCON_BASENAME="%DEVCON_BASENAME%" --var=EXTRA_C_DEFINES="%EXTRA_C_DEFINES%" --var=CODESIGN_PKCS12="%CODESIGN_PKCS12%" --var=CODESIGN_PASS="%CODESIGN_PASS%" --var=CODESIGN_CROSS="%CODESIGN_CROSS%" --var=CODESIGN_TIMESTAMP="%CODESIGN_TIMESTAMP%" --var=CODESIGN_ISTEST="%CODESIGN_ISTEST%" --var=OUTDIR="%OUTDIR%"

for %%f in (config-env.bat src\SOURCES src\config.h) do (
	%msvcg_args% --input=%%f.in --output=%%f
	if errorlevel 1 goto error
)

for %%a in (i386 amd64) do (
	mkdir src\%%a > nul 2>&1
	%msvcg_args% --config=build\vars.%%a.m4 --input=src\OemWin2k.inf.in --output=src\%%a\OemWin2k.inf
	if errorlevel 1 goto error
)

set rc=0
goto end

:error
echo FAILED
set rc=1
goto end

:end

endlocal

exit /b %rc%
