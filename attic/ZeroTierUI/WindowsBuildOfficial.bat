REM If you're an OSS user and want to run this batch file, edit to change the path to Qt.
REM Start Visual Studio command prompt and run this from within the ZeroTierUI subfolder.

CD ..
MKDIR build-ZeroTierUI-win32-release
CD build-ZeroTierUI-win32-release
C:\Qt\5.2.1\Src\qtbase\bin\qmake.exe ..\ZeroTierUI\ZeroTierUI.pro
nmake clean
nmake
