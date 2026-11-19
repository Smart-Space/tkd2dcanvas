# run_demo.ps1 --
#
# Builds (if needed), starts the anti-aliasing demo with the patched Tk and
# saves a screenshot of it.
#
# Usage:  pwsh -File tests/gdi_aa/run_demo.ps1 [-Rebuild] [-Out file.png]

param(
    [switch]$Rebuild,
    [string]$Out = "$PSScriptRoot\aa_demo.png"
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$src = Join-Path $root "third_party\tcltk-src"
$tkWin = Join-Path $src "tk\win"
$tclWin = Join-Path $src "tcl\win"

function Get-BuildDir {
    param([string]$Dir)
    $dirs = Get-ChildItem $Dir -Directory -Filter "Release_*" -ErrorAction SilentlyContinue
    if (-not $dirs) { throw "no build directory in $Dir" }
    return $dirs[0].FullName
}

$tkOut = Get-BuildDir $tkWin
$tclOut = Get-BuildDir $tclWin

if ($Rebuild) {
    $vcvars = "E:\Visual Studio\VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        $vcvars = (Get-Command vcvars64.bat -ErrorAction SilentlyContinue).Source
    }
    if (-not $vcvars) { throw "vcvars64.bat not found" }
    cmd /c "call `"$vcvars`" >nul 2>&1 && cd /d `"$tkWin`" && nmake -f makefile.vc" |
        Select-Object -Last 20
    if ($LASTEXITCODE -ne 0) { throw "Tk build failed" }
}

$env:PATH = "$tclOut;$tkOut;" + $env:PATH
$env:TCL_LIBRARY = Join-Path $src "tcl\library"
$env:TK_LIBRARY = Join-Path $src "tk\library"
# 让 wish 感知系统 DPI，否则在 200% 缩放的屏幕上窗口会被系统拉伸成糊的。
$env:__COMPAT_LAYER = "HighDpiAware"

$wish = Join-Path $tkOut "wish86t.exe"
if (-not (Test-Path $wish)) { throw "wish86t.exe not found in $tkOut" }

Get-Process wish86t -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

$script = Join-Path $PSScriptRoot "aa_demo.tcl"
Start-Process -FilePath $wish -ArgumentList $script -WorkingDirectory $tkOut | Out-Null
Start-Sleep -Seconds 2

python (Join-Path $root "tools\capture_window.py") "TkGDI-AA-Demo" $Out
$code = $LASTEXITCODE

Get-Process wish86t -ErrorAction SilentlyContinue | Stop-Process -Force
exit $code
