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

if not exist config-env.bat (
	echo please run configure
	exit /b 1
)

setlocal

call config-env.bat

for /d %%d in (src\obj*) do rmdir /q /s %%d > nul 2>&1
for %%f in (src\amd64\*.sys src\i386\*.sys) do del /s %%f > nul 2>&1

cmd /c _build WXP x86
if errorlevel 1 goto error
cmd /c _build WIN7 x64
if errorlevel 1 goto error

call installer\build

set rc=0
goto end

:error
echo FAIL
set rc=1
goto end

:end

endlocal

exit /b %rc%
