@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ========================================
echo  Jigsaw Puzzle - Android APK Build
echo ========================================
echo.

REM ============================================================
REM  Config (edit if your SDK paths differ)
REM ============================================================
set "SDK=D:\android_sdk\sdk"
set "NDK=%SDK%\ndk\25.1.8937393"
set "BT=%SDK%\build-tools\37.0.0"
set "CMAKE=%SDK%\cmake\4.1.2\bin\cmake.exe"
set "NINJA=%SDK%\cmake\4.1.2\bin\ninja.exe"
set "PLATFORM_JAR=%SDK%\platforms\android-34\android.jar"
set "ADB=%SDK%\platform-tools\adb.exe"
set "JAR_BIN=C:\Program Files\Eclipse Adoptium\jdk-17.0.17.10-hotspot\bin\jar.exe"
set "KEYTOOL=C:\Program Files\Eclipse Adoptium\jdk-17.0.17.10-hotspot\bin\keytool.exe"

set "ABIS=arm64-v8a armeabi-v7a"
set "MIN_SDK=24"
set "TARGET_SDK=34"

REM ============================================================
REM  Derived paths
REM ============================================================
set "PROJECT_ROOT=%~dp0.."
set "CPP_DIR=%~dp0app\src\main\cpp"
set "MANIFEST=%~dp0app\src\main\AndroidManifest.xml"
set "ASSETS=%PROJECT_ROOT%\assets"
set "BUILD=%~dp0build"
set "STAGE=%BUILD%\apk-stage"
set "OUT=%~dp0jigsaw.apk"

REM ============================================================
REM  Prerequisite checks
REM ============================================================
set "FAIL=0"
for %%T in ("%CMAKE%" "%NINJA%" "%BT%\aapt2.exe" "%BT%\zipalign.exe" "%BT%\apksigner.bat" "%JAR_BIN%" "%KEYTOOL%" "%PLATFORM_JAR%" "%ADB%") do (
    if not exist %%T (
        echo [ERROR] Missing: %%~T
        set "FAIL=1"
    )
)
if not exist "%NDK%\build\cmake\android.toolchain.cmake" (
    echo [ERROR] NDK toolchain not found: %NDK%\build\cmake\android.toolchain.cmake
    set "FAIL=1"
)
if "%FAIL%"=="1" (
    echo.
    echo Fix the missing tools/paths above and re-run.
    exit /b 1
)

echo [1/4] Building native libraries for: %ABIS%
echo.

for %%A in (%ABIS%) do (
    set "ABI=%%A"
    echo --- ABI: %%A ---

    "%CMAKE%" -S "%CPP_DIR%" -B "%BUILD%\%%A" -G Ninja ^
        -DCMAKE_TOOLCHAIN_FILE="%NDK%\build\cmake\android.toolchain.cmake" ^
        -DANDROID_ABI=%%A ^
        -DANDROID_PLATFORM=android-%MIN_SDK% ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
        -DPLATFORM=Android
    if errorlevel 1 (
        echo [ERROR] CMake configure failed for %%A
        exit /b 1
    )

    "%CMAKE%" --build "%BUILD%\%%A"
    if errorlevel 1 (
        echo [ERROR] Build failed for %%A
        exit /b 1
    )

    if not exist "%BUILD%\%%A\libjigsaw.so" (
        echo [ERROR] libjigsaw.so not produced for %%A
        exit /b 1
    )
    echo --- %%A OK ---
    echo.
)

echo [2/4] Assembling APK...

REM Fresh staging area
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%" 2>nul

REM 2a. Generate launcher icons and compile resources
python "%PROJECT_ROOT%\tools\make_icon.py" --android-res "%STAGE%\res" >nul 2>&1
if not exist "%STAGE%\res\mipmap-xxxhdpi\ic_launcher.png" (
    echo [WARN] Icon generation failed — building without a launcher icon
) else (
    "%BT%\aapt2.exe" compile --dir "%STAGE%\res" -o "%BUILD%\res.zip"
    if errorlevel 1 (
        echo [ERROR] aapt2 compile of resources failed
        exit /b 1
    )
)

REM 2b. Create base APK with binary manifest (native-only app, no dex)
set "RES_ARGS="
if exist "%BUILD%\res.zip" set "RES_ARGS=-R %BUILD%\res.zip"
"%BT%\aapt2.exe" link -o "%BUILD%\unsigned.apk" ^
    --manifest "%MANIFEST%" ^
    -I "%PLATFORM_JAR%" ^
    %RES_ARGS% ^
    --min-sdk-version %MIN_SDK% ^
    --target-sdk-version %TARGET_SDK% ^
    --version-code 1 ^
    --version-name "1.0"
if errorlevel 1 (
    echo [ERROR] aapt2 link failed
    exit /b 1
)

REM 2c. Stage native libs (lib/<abi>/libjigsaw.so)
for %%A in (%ABIS%) do (
    mkdir "%STAGE%\lib\%%A" 2>nul
    copy /y "%BUILD%\%%A\libjigsaw.so" "%STAGE%\lib\%%A\" >nul
)

REM 2d. Stage assets (assets/images/...)
if exist "%ASSETS%" (
    xcopy "%ASSETS%" "%STAGE%\assets" /e /i /q /y >nul
) else (
    echo [WARN] assets folder not found: %ASSETS%
)

REM 2e. Add staged files into the APK
pushd "%STAGE%"
"%JAR_BIN%" uf "%BUILD%\unsigned.apk" lib assets
if errorlevel 1 (
    popd
    echo [ERROR] jar update failed
    exit /b 1
)
popd

REM 2f. Align
"%BT%\zipalign.exe" -f 4 "%BUILD%\unsigned.apk" "%BUILD%\aligned.apk"
if errorlevel 1 (
    echo [ERROR] zipalign failed
    exit /b 1
)

REM 2g. Debug keystore (auto-created on first run)
set "KEYSTORE=%BUILD%\debug.keystore"
if not exist "%KEYSTORE%" (
    echo Creating debug keystore...
    "%KEYTOOL%" -genkeypair ^
        -keystore "%KEYSTORE%" ^
        -alias androiddebugkey ^
        -keyalg RSA -keysize 2048 -validity 10000 ^
        -storepass android -keypass android ^
        -dname "CN=Android Debug,O=Android,C=US"
    if errorlevel 1 (
        echo [ERROR] keytool failed
        exit /b 1
    )
)

REM 2h. Sign
"%BT%\apksigner.bat" sign ^
    --ks "%KEYSTORE%" ^
    --ks-pass pass:android ^
    --key-pass pass:android ^
    --out "%OUT%" ^
    "%BUILD%\aligned.apk"
if errorlevel 1 (
    echo [ERROR] apksigner failed
    exit /b 1
)

echo.
echo [3/4] APK created: %OUT%

REM ============================================================
REM  4. Optional: install to a connected device
REM ============================================================
"%ADB%" devices | findstr /r "device$" >nul 2>&1
if errorlevel 1 (
    echo.
    echo [4/4] No device connected. Install manually:
    echo     %ADB% install -r "%OUT%"
) else (
    echo.
    echo [4/4] Device found, installing...
    "%ADB%" install -r "%OUT%"
    if errorlevel 1 (
        echo [WARN] Install failed
    ) else (
        echo.
        echo ========================================
        echo  Installed! Launch:
        echo     %ADB% shell monkey -p io.github.puzzle.jigsaw 1
        echo ========================================
    )
)

echo.
echo Done.
exit /b 0
