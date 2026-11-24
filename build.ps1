# OCreatures build script.
# Drives the build through the VS 2022 (v143) developer environment — NOT VS 2026 (14.51 is too
# new for the pinned deps). vcpkg is separately pinned to VS 2022 in cmake/x64-windows-skse.cmake.
#
# First-time setup (git is user-driven — run these yourself):
#   git init
#   git submodule add https://github.com/alandtse/CommonLibVR.git extern/CommonLibSSE-NG
#   git -C extern/CommonLibSSE-NG checkout ng
#   git submodule update --init --recursive

$ErrorActionPreference = "Continue"

# vswhere (for the dev shell) + our standalone ninja must be on PATH
$env:PATH = "C:\Program Files (x86)\Microsoft Visual Studio\Installer;D:\Development\tools;" + $env:PATH

$vs = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"

$env:VCPKG_ROOT = "D:\Development\vcpkg"

$tmp = Join-Path $PSScriptRoot "build\tmp"
New-Item -ItemType Directory -Force $tmp | Out-Null
$env:TEMP = $tmp
$env:TMP = $tmp
Set-Location $PSScriptRoot

Write-Host "cl.exe  = $((Get-Command cl.exe -ErrorAction SilentlyContinue).Source)"
Write-Host "ninja   = $((Get-Command ninja -ErrorAction SilentlyContinue).Source)"

cmake --preset build-release-msvc
if ($LASTEXITCODE -ne 0) { Write-Host "CONFIGURE FAILED ($LASTEXITCODE)"; exit $LASTEXITCODE }

cmake --build --preset release-msvc
exit $LASTEXITCODE
