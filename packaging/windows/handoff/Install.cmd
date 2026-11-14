@echo off
rem Double-click this to install Debrief for your Windows account. No administrator rights are needed.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install.ps1" %*
if errorlevel 1 (
  echo.
  echo Debrief was not installed. The message above says why.
)
echo.
pause
