# Installs Debrief for the current user. Nothing here needs administrator rights:
# the app goes in %LOCALAPPDATA%\Programs\Debrief, with a Start menu shortcut and
# an entry under Settings > Apps > Installed apps.
#
#   Install.ps1                  asks whether to start Debrief at sign-in
#   Install.ps1 -StartAtSignIn   yes, without asking
#   Install.ps1 -NoStartAtSignIn no, without asking
#   Install.ps1 -NoLaunch        don't open Debrief afterwards
param(
    [switch]$StartAtSignIn,
    [switch]$NoStartAtSignIn,
    [switch]$NoLaunch
)

$ErrorActionPreference = 'Stop'
$version = '0.1.0'
$source = Join-Path $PSScriptRoot 'app'
$target = Join-Path $env:LOCALAPPDATA 'Programs\Debrief'
$exe = Join-Path $target 'Debrief.exe'
$startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Debrief.lnk'
$startup = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Startup\Debrief.lnk'
$uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Debrief'

if (-not (Test-Path (Join-Path $source 'Debrief.exe'))) {
    Write-Host "Can't find app\Debrief.exe next to this script."
    Write-Host "Extract the whole zip first (right-click it, Extract All), then run Install.cmd from the extracted folder."
    exit 1
}
if (-not [Environment]::Is64BitOperatingSystem) {
    Write-Host 'Debrief needs 64-bit Windows.'
    exit 1
}

# An update: the running copy has its files open, so it goes first.
$running = Get-Process -Name 'Debrief' -ErrorAction SilentlyContinue
if ($running) {
    Write-Host 'Debrief is running; closing it to update it.'
    $running | Stop-Process -Force
    Start-Sleep -Seconds 1
}

Write-Host "Installing Debrief $version to $target"
if (Test-Path $target) {
    Remove-Item -Path $target -Recurse -Force
}
New-Item -ItemType Directory -Path $target -Force | Out-Null
Copy-Item -Path (Join-Path $source '*') -Destination $target -Recurse -Force
foreach ($extra in 'README.txt', 'docs', 'licenses') {
    $path = Join-Path $PSScriptRoot $extra
    if (Test-Path $path) {
        Copy-Item -Path $path -Destination $target -Recurse -Force
    }
}

# Files from a downloaded zip are marked as coming from the internet, which
# makes Windows ask about each one. They are this app's own files.
Get-ChildItem -Path $target -Recurse -File | Unblock-File -ErrorAction SilentlyContinue

function New-Shortcut($path, $arguments, $description) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($path)
    $shortcut.TargetPath = $exe
    $shortcut.Arguments = $arguments
    $shortcut.WorkingDirectory = $target
    $shortcut.IconLocation = "$exe,0"
    $shortcut.Description = $description
    $shortcut.Save()
}

New-Shortcut $startMenu '' 'Record a debugging session with voice-triggered screenshots'

$atSignIn = $StartAtSignIn.IsPresent
if (-not $StartAtSignIn -and -not $NoStartAtSignIn) {
    $answer = Read-Host 'Start Debrief quietly in the notification area each time you sign in? [Y/n]'
    $atSignIn = $answer -notmatch '^\s*n'
}
if ($atSignIn) {
    New-Shortcut $startup '--hidden' 'Debrief, waiting in the notification area'
} elseif (Test-Path $startup) {
    Remove-Item $startup -Force
}

$size = [int]((Get-ChildItem -Path $target -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1KB)
New-Item -Path $uninstallKey -Force | Out-Null
Set-ItemProperty -Path $uninstallKey -Name 'DisplayName' -Value 'Debrief'
Set-ItemProperty -Path $uninstallKey -Name 'DisplayVersion' -Value $version
Set-ItemProperty -Path $uninstallKey -Name 'Publisher' -Value 'Debrief'
Set-ItemProperty -Path $uninstallKey -Name 'InstallLocation' -Value $target
Set-ItemProperty -Path $uninstallKey -Name 'DisplayIcon' -Value "$exe,0"
Set-ItemProperty -Path $uninstallKey -Name 'UninstallString' -Value "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$target\Uninstall.ps1`""
Set-ItemProperty -Path $uninstallKey -Name 'EstimatedSize' -Value $size -Type DWord
Set-ItemProperty -Path $uninstallKey -Name 'NoModify' -Value 1 -Type DWord
Set-ItemProperty -Path $uninstallKey -Name 'NoRepair' -Value 1 -Type DWord

Write-Host ''
Write-Host 'Debrief is installed.'
Write-Host '  Open it:      Start menu > Debrief'
Write-Host "  Sessions:     $([Environment]::GetFolderPath('MyDocuments'))\Debrief"
Write-Host '  Remove it:    Settings > Apps > Installed apps > Debrief > Uninstall'
Write-Host "  Read more:    $target\README.txt"

if (-not $NoLaunch) {
    Start-Process -FilePath $exe -WorkingDirectory $target
}
exit 0
