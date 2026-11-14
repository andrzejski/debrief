@echo off
rem Removes Debrief. Your sessions in Documents\Debrief are kept.
copy /y "%~dp0Uninstall.ps1" "%TEMP%\Debrief-Uninstall.ps1" >nul
cd /d "%TEMP%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%TEMP%\Debrief-Uninstall.ps1"
