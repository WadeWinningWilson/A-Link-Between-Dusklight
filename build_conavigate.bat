@echo off
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
cmake --preset windows-msvc-relwithdebinfo -DDUSK_ENABLE_CONAVIGATE=ON
if errorlevel 1 exit /b 1
call build_run.bat
