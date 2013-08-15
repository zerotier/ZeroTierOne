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

setlocal enableextensions enabledelayedexpansion

set root=%cd%
set myos=%1
set myprofile=%2
set mymode=fre

echo Building %myos%-%myprofile%-%mymode%

call "%DDK%\bin\setenv" %DDK% %mymode% %myprofile% %myos% no_oacr
if errorlevel 1 goto error

cd /d %root%
cd src
nmake
if errorlevel 1 goto error

set rc=0
goto end

:error
echo FAIL %myos%-%myprofile%-%mymode%
set rc=1
goto end

:end

endlocal
