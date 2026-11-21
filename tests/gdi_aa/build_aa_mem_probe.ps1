# build_aa_mem_probe.ps1 --
#
# Builds and runs the memory probe of the anti-aliasing engine
# (tests/gdi_aa/aa_mem_probe.cpp).  No window, no Tk, no Python.
#
# Usage:  powershell -ExecutionPolicy Bypass -File tests\gdi_aa\build_aa_mem_probe.ps1 -Run

param(
    [switch]$Run,
    [int]$Frames = 400,
    [int]$Shapes = 30
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

$bat = Join-Path $objDir "build_aa_mem_probe.bat"
$batContent = @"
@echo off
call "$vcvars" >nul 2>&1
cd /d "$root"
cl /nologo /W3 /O2 /EHsc /std:c++17 /utf-8 /DNOMINMAX /DUNICODE /D_UNICODE /I "$root\native\include" /I "$root\native\render" /Fo"$objDir\\" /Fe"$objDir\aa_mem_probe.exe" "$PSScriptRoot\aa_mem_probe.cpp" "$root\native\render\GdiAntialias.cpp" "$root\native\render\GdiScratchPool.cpp" /link gdi32.lib user32.lib msimg32.lib psapi.lib
exit /b %ERRORLEVEL%
"@
Set-Content -Path $bat -Value $batContent -Encoding ASCII

$outFile = Join-Path $objDir "build.log"
cmd /c $bat > $outFile 2>&1
if ($LASTEXITCODE -ne 0) {
    Get-Content $outFile
    throw "build failed"
}
Write-Host "built $objDir\aa_mem_probe.exe"

if ($Run) {
    & "$objDir\aa_mem_probe.exe" $Frames $Shapes
    if ($LASTEXITCODE -ne 0) { throw "probe failed" }
}
