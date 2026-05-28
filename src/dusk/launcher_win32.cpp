/**
 * Thin Windows launcher EXE. The game lives in dusk.dll, this just forwards
 * the Windows entry point to it. Keeping the game as a DLL lets mod .dll
 * files link against dusk.lib and resolve all game symbols at load time.
 */

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// see src/dusk/main.cpp
int dusk_WinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show);

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show) {
    return dusk_WinMain(hInst, hPrev, cmd, show);
}
