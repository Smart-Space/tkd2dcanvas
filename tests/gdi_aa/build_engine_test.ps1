# build_engine_test.ps1 --
#
# 编译并运行 tkWinAA.c 的独立测试（不需要窗口）。
#
# 用法:  pwsh -File tests/gdi_aa/build_engine_test.ps1

param(
    [switch]$Run
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$src = Join-Path $root "third_party\tcltk-src"
$tclWin = Join-Path $src "tcl\win"

$tclOut = (Get-ChildItem $tclWin -Directory -Filter "Release_*")[0].FullName
$tclLib = Join-Path $tclOut "tcl86t.lib"
if (-not (Test-Path $tclLib)) {
    throw "找不到 $tclLib"
}

$vcvars = "E:\Visual Studio\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    $vcvars = (Get-Command vcvars64.bat -ErrorAction SilentlyContinue).Source
}
if (-not $vcvars) { throw "找不到 vcvars64.bat" }

$objDir = Join-Path $PSScriptRoot "build"
New-Item -ItemType Directory -Force $objDir | Out-Null

# 生成一个临时批处理文件，避免命令行太长或者换行带来的问题。
$bat = Join-Path $objDir "build_engine_test.bat"
$batContent = @"
@echo off
call "$vcvars" >nul 2>&1
cd /d "$root"
cl /nologo /W3 /O2 /DSTDC_HEADERS /DTCL_THREADS=1 /DUSE_THREAD_ALLOC=1 /Dinline=__inline /I "$src\tcl\generic" /I "$src\tcl\win" /I "$src\tk\generic" /I "$src\tk\win" /I "$src\tk\xlib" /I "$src\tk\bitmaps" /Fo"$objDir\\" /Fe"$objDir\aa_engine_test.exe" "$PSScriptRoot\aa_engine_test.c" "$src\tk\win\tkWinAA.c" /link "$tclLib" gdi32.lib user32.lib
exit /b %ERRORLEVEL%
"@
Set-Content -Path $bat -Value $batContent -Encoding ASCII

$outFile = Join-Path $objDir "build.log"
cmd /c $bat > $outFile 2>&1
if ($LASTEXITCODE -ne 0) {
    Get-Content $outFile
    throw "编译失败"
}
Write-Host "已生成 $objDir\aa_engine_test.exe"

if ($Run) {
    $env:PATH = "$tclOut;" + $env:PATH
    Push-Location $objDir
    try {
        & "$objDir\aa_engine_test.exe"
        if ($LASTEXITCODE -ne 0) { throw "测试失败" }
    } finally {
        Pop-Location
    }
}
