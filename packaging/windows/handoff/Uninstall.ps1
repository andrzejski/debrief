# Removes Debrief for the current user: the app, its shortcuts, and its entry
# under Installed apps. Your sessions in Documents\Debrief are yours and stay.
param([switch]$Quiet)

$ErrorActionPreference = 'Continue'
$target = Join-Path $env:LOCALAPPDATA 'Programs\Debrief'

Get-Process -Name 'Debrief' -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

Remove-Item (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Debrief.lnk') -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Startup\Debrief.lnk') -Force -ErrorAction SilentlyContinue
Remove-Item 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Debrief' -Recurse -Force -ErrorAction SilentlyContinue
# The log.
Remove-Item (Join-Path $env:LOCALAPPDATA 'debrief') -Recurse -Force -ErrorAction SilentlyContinue

# This script lives in the folder it removes, so step out of it first.
Set-Location $env:TEMP
if (Test-Path $target) {
    Remove-Item -Path $target -Recurse -Force -ErrorAction SilentlyContinue
}

if (-not $Quiet) {
    Write-Host 'Debrief has been removed. Your sessions are still in Documents\Debrief.'
    Start-Sleep -Seconds 3
}
