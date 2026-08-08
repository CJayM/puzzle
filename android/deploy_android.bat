@echo off
setlocal enabledelayedexpansion

echo ========================================
echo  Jigsaw Puzzle - Deploy APK to device
echo ========================================
echo.

REM === Config ===
set "SDK=D:\android_sdk\sdk"
set "ADB=%SDK%\platform-tools\adb.exe"

REM APK lives next to this script (android/jigsaw.apk)
set "APK=%~dp0jigsaw.apk"

if not exist "%APK%" (
    echo [ERROR] APK not found: %APK%
    echo Run build-android.bat first.
    exit /b 1
)

if not exist "%ADB%" (
    echo [ERROR] adb not found: %ADB%
    exit /b 1
)

echo [1/3] Checking devices...
"%ADB%" devices
"%ADB%" devices > "%TEMP%\jigsaw_adb.txt" 2>&1

REM Find the first authorized device ("...  device", not unauthorized/offline)
set "DEVICE="
for /f "tokens=1" %%d in ('findstr /r "device$" "%TEMP%\jigsaw_adb.txt"') do (
    if not defined DEVICE set "DEVICE=%%d"
)
del "%TEMP%\jigsaw_adb.txt" >nul 2>&1

if not defined DEVICE (
    echo.
    echo [ERROR] No authorized device connected.
    echo Connect the phone and confirm the USB debugging dialog.
    exit /b 1
)

echo.
echo Device: %DEVICE%
echo [2/3] Installing %APK% ...
"%ADB%" -s "%DEVICE%" install -r "%APK%"
if errorlevel 1 (
    echo [ERROR] Install failed.
    exit /b 1
)

echo.
echo [3/3] Installed successfully!
echo Launch with:
echo     "%ADB%" -s "%DEVICE%" shell monkey -p io.github.puzzle.jigsaw 1
echo.
exit /b 0
