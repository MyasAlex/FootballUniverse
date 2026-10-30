@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] CMake not found. Install Visual Studio 2022 with C++ and CMake.
  pause
  exit /b 1
)
echo Building Football Universe for 64-bit Windows...
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
  echo [ERROR] CMake configuration failed. Check Visual Studio C++ workload.
  pause
  exit /b 1
)
cmake --build build-windows --config Release
if errorlevel 1 (
  echo [ERROR] Compilation failed. Review the diagnostics above.
  pause
  exit /b 1
)
echo.
echo SUCCESS: build-windows\Release\FootballUniverse.exe
echo.
pause
