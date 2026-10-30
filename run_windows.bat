@echo off
setlocal
cd /d "%~dp0"
if not exist "build-windows\Release\FootballUniverse.exe" (
  echo Executable not found. Starting Windows build...
  call build_windows.bat
)
if exist "build-windows\Release\FootballUniverse.exe" (
  cd /d "build-windows\Release"
  start "" "FootballUniverse.exe"
) else (
  echo Could not start. Please read README.md.
  pause
)
