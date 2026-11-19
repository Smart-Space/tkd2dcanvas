# build_tk_aa.ps1 --
#
# 把"纯 GDI 手动抗锯齿"的补丁应用到 Tcl/Tk 源码，并重新编译 Tk。
#
# 用法:
#     pwsh -File tools\build_tk_aa.ps1 [-Force]
#
#   -Force  即使补丁已经打过，也重新打一次（先还原再打）

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$src = Join-Path $root "third_party\tcltk-src"
$tkSrc = Join-Path $src "tk"
$tkWin = Join-Path $tkSrc "win"
$patch = Join-Path $root "third_party\patches\tk-8.6-gdi-antialias.patch"

if (-not (Test-Path $tkSrc)) {
    throw "找不到 Tcl/Tk 源码：$tkSrc（见 docs/building.md）"
}
if (-not (Test-Path $patch)) {
    throw "找不到补丁：$patch"
}

# --- 应用补丁 ------------------------------------------------------------

$applied = Select-String -Path (Join-Path $tkSrc "win\tkWinDraw.c") `
    -Pattern "TkWinAAIsActive" -Quiet -ErrorAction SilentlyContinue

if ($applied -and -not $Force) {
    Write-Host "补丁已经打过，跳过（用 -Force 可以重新打）"
} else {
    if ($applied) {
        Write-Host "先还原补丁..."
        cmd /c "cd /d `"$tkSrc`" && git checkout -- generic/tkCanvas.c generic/tkCanvas.h win/makefile.vc win/tkWinDraw.c" | Out-Null
        cmd /c "cd /d `"$tkSrc`" && git clean -f -- win/tkWinAA.c win/tkWinAA.h" | Out-Null
    }
    Write-Host "应用补丁 $patch ..."
    cmd /c "cd /d `"$tkSrc`" && git apply `"$patch`""
    if ($LASTEXITCODE -ne 0) {
        throw "补丁应用失败"
    }
}

# --- 编译 ---------------------------------------------------------------

$vcvars = "E:\Visual Studio\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    $vcvars = (Get-Command vcvars64.bat -ErrorAction SilentlyContinue).Source
}
if (-not $vcvars) { throw "找不到 vcvars64.bat" }

Write-Host "编译 Tk ..."
cmd /c "call `"$vcvars`" >nul 2>&1 && cd /d `"$tkWin`" && nmake -f makefile.vc"
if ($LASTEXITCODE -ne 0) {
    throw "Tk 编译失败"
}

$outDir = (Get-ChildItem $tkWin -Directory -Filter "Release_*")[0].FullName
Write-Host "完成：$outDir\tk86t.dll"
Write-Host "测试：pwsh -File tests\gdi_aa\build_engine_test.ps1 -Run"
Write-Host "演示：pwsh -File tests\gdi_aa\run_demo.ps1 -Out aa_demo.png"
