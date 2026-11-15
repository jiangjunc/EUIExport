@echo off
rem Build the single-file eui.exe with MinGW-w64 gcc.
rem Works from a plain cmd.exe, PowerShell, or the MSYS2 shell.
setlocal
cd /d "%~dp0"

where gcc >nul 2>nul
if errorlevel 1 (
  if exist "C:\msys64\ucrt64\bin\gcc.exe" set "PATH=C:\msys64\ucrt64\bin;%PATH%"
)
where gcc >nul 2>nul
if errorlevel 1 (
  if exist "C:\msys64\mingw64\bin\gcc.exe" set "PATH=C:\msys64\mingw64\bin;%PATH%"
)
where gcc >nul 2>nul
if errorlevel 1 (
  echo [error] gcc not found. Install MSYS2 ^(ucrt64 or mingw64^) and retry.
  exit /b 1
)

if not exist build mkdir build

set CFLAGS=-std=c11 -Os -Wall -Wextra -Ivendor\zstd -DZSTD_DISABLE_ASM -DZSTD_LEGACY_SUPPORT=0 -DZSTD_STRIP_ERROR_STRINGS
set LDFLAGS=-static -s -ffunction-sections -fdata-sections -Wl,--gc-sections -lshell32

echo [build] gcc src\*.c vendor\zstd\*.c -^> build\eui.exe
gcc %CFLAGS% src\*.c vendor\zstd\common\*.c vendor\zstd\decompress\*.c -o build\eui.exe %LDFLAGS%
if errorlevel 1 exit /b 1

echo [done] build\eui.exe
for %%F in (build\eui.exe) do echo        %%~zF bytes
endlocal
