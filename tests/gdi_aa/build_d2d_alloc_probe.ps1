# build_d2d_alloc_probe.ps1 --
#
# Builds and runs the probe that shows what the Direct2D back end allocates
# (text layouts, path geometries, bitmaps) and how the private commit
# behaves while it repeats one operation.  No window is needed.
#
# Usage:  powershell -ExecutionPolicy Bypass -File tests/gdi_aa/build_d2d_alloc_probe.ps1 -Run
#
# The comments are kept in ASCII on purpose: Windows PowerShell 5.1 reads
# .ps1 files as ANSI unless they carry a UTF-8 BOM.

param(
    [switch]$Run,
    [string]$Mode = "text",
    [int]$Iterations = 2000
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

$bat = Join-Path $objDir "build_d2d_alloc_probe.bat"
$batContent = @"
@echo off
call "$vcvars" >nul 2>&1
cd /d "$root"
cl /nologo /W3 /O2 /EHsc /std:c++17 /utf-8 /DNOMINMAX /DUNICODE /D_UNICODE /I "$root\native\include" /I "$root\native\render" /Fo"$objDir\\" /Fe"$objDir\d2d_alloc_probe.exe" "$PSScriptRoot\d2d_alloc_probe.cpp" "$root\native\render\D2DContext.cpp" "$root\native\render\D2DTargetPool.cpp" /link d2d1.lib dwrite.lib gdi32.lib user32.lib psapi.lib
exit /b %ERRORLEVEL%
"@
Set-Content -Path $bat -Value $batContent -Encoding ASCII

$outFile = Join-Path $objDir "build_d2d_alloc.log"
cmd /c $bat > $outFile 2>&1
if ($LASTEXITCODE -ne 0) {
    Get-Content $outFile
    throw "build failed"
}
Write-Host "built $objDir\d2d_alloc_probe.exe"

if ($Run) {
    & "$objDir\d2d_alloc_probe.exe" $Mode $Iterations
    if ($LASTEXITCODE -ne 0) { throw "probe failed" }
}
