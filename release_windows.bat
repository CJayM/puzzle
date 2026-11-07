@echo off
REM ============================================================
REM  Сборка релизной Windows-версии и упаковка в zip
REM  Версия берётся из корневого CMakeLists.txt
REM ============================================================
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo ========================================
echo  Jigsaw Puzzle - Windows release
echo ========================================
echo.

REM ---- Config ----
set "W64DEVKIT=C:\raylib\w64devkit\bin"
set "SEVENZIP=C:\Program Files\7-Zip\7z.exe"

REM ---- Версия из CMakeLists.txt (единственный источник) ----
set "APPVER="
for /f "tokens=3" %%V in ('findstr /b /c:"project(JigsawPuzzle VERSION" "%~dp0CMakeLists.txt"') do set "APPVER=%%V"
if not defined APPVER (
    echo [ERROR] Не удалось прочитать версию из CMakeLists.txt
    echo         Ожидается строка: project JigsawPuzzle VERSION x.y.z
    exit /b 1
)

set "BUILD=%~dp0build\Raylib_gcc-Release"
set "DIST=%~dp0puzzle_windows_%APPVER%.zip"

echo Версия: %APPVER%
echo Архив:  %DIST%
echo.

REM ---- Занят только релизный exe (если запущена Debug-копия, это не мешает) ----
REM Без явного exit 0 подавленная ошибка Get-Process даёт код 1 при отсутствии процесса
powershell -NoProfile -Command "$p = @(Get-Process JigsawPuzzle -ErrorAction Ignore | Where-Object { $_.Path -like '*Raylib_gcc-Release*' }); if ($p.Count -gt 0) { exit 1 } else { exit 0 }"
if errorlevel 1 (
    echo [ERROR] Запущен релизный JigsawPuzzle.exe - закройте его и повторите.
    exit /b 1
)

if not exist "%W64DEVKIT%\g++.exe" (
    echo [ERROR] Не найден w64devkit: %W64DEVKIT%
    exit /b 1
)
set "PATH=%W64DEVKIT%;%PATH%"

REM ---- Конфигурирование (один раз) ----
if not exist "%BUILD%\CMakeCache.txt" (
    echo [1/3] Конфигурирование релизной сборки...
    cmake -S "%~dp0." -B "%BUILD%" -G Ninja ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_C_COMPILER=C:/raylib/w64devkit/bin/gcc.exe ^
        -DCMAKE_CXX_COMPILER=C:/raylib/w64devkit/bin/g++.exe
    if errorlevel 1 (
        echo [ERROR] Конфигурирование не удалось
        exit /b 1
    )
) else (
    echo [1/3] Каталог сборки уже сконфигурирован: %BUILD%
)

REM ---- Сборка ----
echo [2/3] Сборка Release...
cmake --build "%BUILD%"
if errorlevel 1 (
    echo [ERROR] Сборка не удалась
    exit /b 1
)

if not exist "%BUILD%\JigsawPuzzle.exe" (
    echo [ERROR] Нет файла %BUILD%\JigsawPuzzle.exe
    exit /b 1
)
if not exist "%BUILD%\assets\images\catalog.txt" (
    echo [WARN] В каталоге сборки нет assets\images\catalog.txt - проверьте CopyAssets
)

REM README кладём в архив рядом с exe
copy /y "%~dp0README.md" "%BUILD%\README.md" >nul 2>&1

REM ---- Упаковка ----
echo [3/3] Упаковка архива...
if exist "%DIST%" del /q "%DIST%"

if exist "%SEVENZIP%" goto :zip7z
goto :zipps

:zip7z
pushd "%BUILD%"
"%SEVENZIP%" a -tzip -mx=1 -mmt "%DIST%" "JigsawPuzzle.exe" "README.md" "assets" -xr!_meta_check* >nul
if errorlevel 1 (
    popd
    echo [ERROR] 7-Zip не смог создать архив
    exit /b 1
)
popd
goto :done

:zipps
echo [WARN] 7-Zip не найден, использую Compress-Archive (медленнее)
powershell -NoProfile -Command "Compress-Archive -Path '%BUILD%\JigsawPuzzle.exe','%BUILD%\README.md','%BUILD%\assets' -DestinationPath '%DIST%' -Force -CompressionLevel Fastest"
if errorlevel 1 (
    echo [ERROR] Compress-Archive не смог создать архив
    exit /b 1
)

:done
if not exist "%DIST%" (
    echo [ERROR] Архив не создан: %DIST%
    exit /b 1
)

for %%F in ("%DIST%") do set "SIZE=%%~zF"
set /a SIZEMB=SIZE/1048576

echo.
echo ========================================
echo  Готово: %DIST%
echo  Размер: %SIZEMB% МБ
echo ========================================
exit /b 0
