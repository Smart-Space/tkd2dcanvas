# build_aa_render_test.ps1 --
#
# Builds and runs the unit test of this repository's anti-aliasing engine
# (native/render/GdiAntialias.cpp).  No window is needed.
#
# Usage:  powershell -ExecutionPolicy Bypass -File tests/gdi_aa/build_aa_render_test.ps1
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
$bat = Join-Path $objDir "build_aa_render_test.bat"
$batContent = @"
@echo off
call "$vcvars" >nul 2>&1
cd /d "$root"
cl /nologo /W3 /O2 /EHsc /std:c++17 /utf-8 /DNOMINMAX /DUNICODE /D_UNICODE /I "$root\native\include" /I "$root\native\render" /Fo"$objDir\\" /Fe"$objDir\aa_render_test.exe" "$PSScriptRoot\aa_render_test.cpp" "$root\native\render\GdiAntialias.cpp" /link gdi32.lib user32.lib msimg32.lib
exit /b %ERRORLEVEL%
"@
Set-Content -Path $bat -Value $batContent -Encoding ASCII

$outFile = Join-Path $objDir "build.log"
cmd /c $bat > $outFile 2>&1
if ($LASTEXITCODE -ne 0) {
    Get-Content $outFile
    throw "build failed"
}
Write-Host "built $objDir\aa_render_test.exe"

if ($Run) {
    & "$objDir\aa_render_test.exe"
    if ($LASTEXITCODE -ne 0) { throw "test failed" }
}


