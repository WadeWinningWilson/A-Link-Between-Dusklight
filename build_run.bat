@echo off
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
if not exist "%~dp0build\windows-msvc-relwithdebinfo\build.ninja" (
    cmake --preset windows-msvc-relwithdebinfo
)
ninja -j8 -C "%~dp0build\windows-msvc-relwithdebinfo" dusklight
if errorlevel 1 exit /b 1
for %%F in ("%~dp0build\windows-msvc-relwithdebinfo\dusklight.exe") do set EXESIZE=%%~zF
if "%EXESIZE%"=="0" (
    echo ERROR: dusklight.exe is 0 bytes — link failed or AV blocked the file. Try rebuild.bat
    exit /b 1
)
