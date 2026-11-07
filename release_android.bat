@echo off
REM ============================================================
REM  Сборка релизного Android APK и копия в puzzle_android_<version>.apk
REM  Версия берётся из корневого CMakeLists.txt
REM  Сборку делает android\build-android.bat, здесь только копия и проверка
REM ============================================================
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo ========================================
echo  Jigsaw Puzzle - Android release
echo ========================================
echo.

REM ---- Инструменты Android (вне проекта, абсолютные пути) ----
set "BT=D:\android_sdk\sdk\build-tools\37.0.0"
set "AAPT2=%BT%\aapt2.exe"
set "APKSIGNER=%BT%\apksigner.bat"

REM ---- Версия из CMakeLists.txt (единственный источник) ----
set "APPVER="
for /f "tokens=3" %%V in ('findstr /b /c:"project(JigsawPuzzle VERSION" "%~dp0CMakeLists.txt"') do set "APPVER=%%V"
if not defined APPVER (
    echo [ERROR] Не удалось прочитать версию из CMakeLists.txt
    echo         Ожидается строка: project JigsawPuzzle VERSION x.y.z
    exit /b 1
)

set "APK=%~dp0android\jigsaw.apk"
set "DIST=%~dp0puzzle_android_%APPVER%.apk"

echo Версия: %APPVER%
echo Файл:   %DIST%
echo.

if not exist "%~dp0android\build-android.bat" (
    echo [ERROR] Не найден android\build-android.bat
    exit /b 1
)

REM ---- Старый артефакт удаляем: лог сборки обрывается на упаковке,
REM ---- поэтому о успехе судим по свежему файлу, а не по тексту лога ----
if exist "%APK%" del /q "%APK%"

echo [1/2] Сборка APK...
set "NO_INSTALL=1"
call "%~dp0android\build-android.bat"
set "RC=%errorlevel%"
cd /d "%~dp0"

if not "%RC%"=="0" (
    echo [ERROR] Сборка Android завершилась с кодом %RC%
    exit /b 1
)
if not exist "%APK%" (
    echo [ERROR] APK не создан: %APK%
    exit /b 1
)

echo [2/2] Копия и проверка...
copy /y "%APK%" "%DIST%" >nul
if errorlevel 1 (
    echo [ERROR] Не удалось скопировать APK
    exit /b 1
)

REM call обязателен: прямой вызов .bat не возвращает управление
call "%APKSIGNER%" verify "%DIST%"
if errorlevel 1 (
    echo [ERROR] Подпись APK не прошла проверку
    exit /b 1
)

echo.
echo Манифест:
"%AAPT2%" dump badging "%DIST%" | findstr /c:"versionName"

for %%F in ("%DIST%") do set "SIZE=%%~zF"
set /a SIZEMB=SIZE/1048576

echo.
echo ========================================
echo  Готово: %DIST%
echo  Размер: %SIZEMB% МБ
echo ========================================
exit /b 0
