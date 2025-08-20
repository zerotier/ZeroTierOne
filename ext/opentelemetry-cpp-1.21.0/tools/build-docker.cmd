REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

@echo off
pushd %~dp0
REM Default arguments
set A1="/build/tools/build.sh"
set A2=clean
if "%~1"=="" goto help

REM Image name is always the first argument
set IMAGE_NAME=%1

REM Process other optional arguments
shift
if NOT "%1"=="" (
  set A1=%1
  set A2=%2
  set A3=%3
  set A4=%4
  set A5=%5
  set A6=%6
  set A7=%7
  set A8=%8
  set A9=%9
  set A10=%10
)

WHERE choco >NUL 2>NUL
if %ERRORLEVEL% NEQ 0 (
  echo This script requires Chocolatey to install Docker: https://chocolatey.org/
)

WHERE docker >NUL 2>NUL
if "%ERRORLEVEL%"=="0" goto docker_ok
choco install -y docker-desktop
choco install -y docker-cli
:docker_ok

docker info
docker version

echo Running in container %IMAGE_NAME%
sc query com.docker.service

REM Force reinstallation of build tools
del ..\.buildtools 2>NUL
echo Building docker image in %CD%...
copy /Y setup-cmake.sh ..\docker\%IMAGE_NAME%\
copy /Y setup-protobuf.sh ..\docker\%IMAGE_NAME%\
docker build --rm -t %IMAGE_NAME% ../docker/%IMAGE_NAME%

cd ..
echo Starting build...
docker run -it -v %CD%:/build %IMAGE_NAME% %A1% %A2%

popd
exit

:help
cd ..
echo.
echo Usage: build-docker.cmd [image_name] [arguments...]
echo. 
echo Default command:
echo. 
echo  docker run -it -v $WORKSPACE_ROOT:/build IMAGE_NAME %A1% %A2%
echo.
echo Supported images:
echo =================
dir /B docker
popd
