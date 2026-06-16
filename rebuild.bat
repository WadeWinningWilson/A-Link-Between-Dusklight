@echo off
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo Failed to load MSVC environment.
    exit /b 1
)
echo Cleaning dusklight...
ninja -C "%~dp0build\windows-msvc-relwithdebinfo" -t clean dusklight
if errorlevel 1 exit /b 1
echo Building dusklight...
ninja -j8 -C "%~dp0build\windows-msvc-relwithdebinfo" dusklight
if errorlevel 1 exit /b 1
echo.
echo Build OK: %~dp0build\windows-msvc-relwithdebinfo\dusklight.exe
dir "%~dp0build\windows-msvc-relwithdebinfo\dusklight.exe"
