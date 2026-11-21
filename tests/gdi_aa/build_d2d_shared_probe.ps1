# build_d2d_shared_probe.ps1 --
#
# Builds and runs the probe that shows that the Direct2D render target is
# shared by the whole process (and not one per canvas widget).  No window is
# needed.
#
# Usage:  powershell -ExecutionPolicy Bypass -File tests/gdi_aa/build_d2d_shared_probe.ps1
#
# The comments are kept in ASCII on purpose: Windows PowerShell 5.1 reads
# .ps1 files as ANSI unless they carry a UTF-8 BOM.

param(
    [switch]$Run
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")

$vcvars = "E:\Visual Studio\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    $vcvars = (Get-Command vcvars64.bat -ErrorAction SilentlyContinue).Source
}
if (-not $vcvars) { throw "vcvars64.bat not found" }

$objDir = Join-Path $PSScriptRoot "build"
New-Item -ItemType Directory -Force $objDir | Out-Null

# A temporary batch file keeps the command line short and free of newlines.
$bat = Join-Path $objDir "build_d2d_shared_probe.bat"
$batContent = @"
@echo off
call "$vcvars" >nul 2>&1
cd /d "$root"
cl /nologo /W3 /O2 /EHsc /std:c++17 /utf-8 /DNOMINMAX /DUNICODE /D_UNICODE /I "$root\native\include" /I "$root\native\render" /Fo"$objDir\\" /Fe"$objDir\d2d_shared_probe.exe" "$PSScriptRoot\d2d_shared_probe.cpp" "$root\native\render\D2DRenderer.cpp" "$root\native\render\D2DContext.cpp" "$root\native\render\D2DTargetPool.cpp" "$root\native\render\FontCache.cpp" "$root\native\render\PenCache.cpp" /link d2d1.lib dwrite.lib gdi32.lib user32.lib
exit /b %ERRORLEVEL%
"@
Set-Content -Path $bat -Value $batContent -Encoding ASCII

$outFile = Join-Path $objDir "build_d2d_shared.log"
cmd /c $bat > $outFile 2>&1
if ($LASTEXITCODE -ne 0) {
    Get-Content $outFile
    throw "build failed"
}
Write-Host "built $objDir\d2d_shared_probe.exe"

if ($Run) {
    & "$objDir\d2d_shared_probe.exe"
    if ($LASTEXITCODE -ne 0) { throw "probe failed" }
}
