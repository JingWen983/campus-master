# ===========================================================================
# tests/run_tests.ps1 —— 一条命令编译并运行 C++ 单元测试
#
# 用法（在仓库根或任意目录均可）：
#   pwsh -NoProfile -File tests/run_tests.ps1
#   pwsh -NoProfile -File tests/run_tests.ps1 -Only rfc7914
#   pwsh -NoProfile -File tests/run_tests.ps1 -List
#
# 退出码：
#   0 = 全部（被选中的）用例与断言通过
#   1 = 有用例/断言失败（会打印每条的 expected / actual）
#   2 = 编译失败
#   3 = 环境或用法错误（找不到 g++、产物缺失、-Only 未匹配任何用例等）
#
# 说明：
#   * 编译命令（与 T2 验收判据一致，仅输出路径改为**每进程唯一**，见下方 T34 说明）：
#       g++ tests/pbkdf2_rfc7914_test.cpp -o tests/pbkdf2_test_<PID>.exe -std=c++17 -O2 -I.
#     为使用相对路径，编译前会切到仓库根目录（$PSScriptRoot 的上一级）。
#   * 安全修复 T34：输出路径**不得**是固定的单文件路径。原实现固定写 tests/pbkdf2_test.exe，
#     当两名成员并行跑本套件时，一方正在执行该 exe（Windows 对运行中的映像持有映像锁），
#     另一方的 ld 就会 `cannot open output file ... Permission denied` → 脚本按设计输出
#     COMPILE FAILED 并 exit 2 —— 极易被误读成「测试坏了/代码坏了」（本批已实际发生一次，
#     并导致一条 verify 记录缺口）。现改为 tests/pbkdf2_test_<PID>.exe（每进程唯一），
#     用完即删；路径保持**相对**（相对仓库根），以免把含非 ASCII 字符的绝对路径作为
#     参数传给 g++；该产物仍落在 tests/ 下，被 .gitignore 的 `tests/*` 覆盖。
#   * 全程只用 PowerShell 直接调用 g++ / 测试 exe，**不经 Node child_process**，
#     也不把子进程输出重定向到管道后再回显（直接流式输出，便于人工与审计复核）。
#   * 在 sha256.h 尚未按 RFC 8018 修复时，本脚本**必须**以非 0 退出：这是
#     BATCH0_CONTRACT.md §3.1 V1-9 要求的「基线必然失败」门禁，不是脚本缺陷。
# ===========================================================================
[CmdletBinding()]
param(
    [string]$Only = '',
    [switch]$List
)

$ErrorActionPreference = 'Stop'

# 让中文输出按 UTF-8 渲染（失败时不影响退出码语义）
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }

$testsDir = $PSScriptRoot
$repoRoot = Split-Path -Parent $testsDir
$src = Join-Path $testsDir 'pbkdf2_rfc7914_test.cpp'
# 安全修复 T34：编译输出必须是**每进程唯一**的路径，否则并发执行会互相加锁而假性失败。
$exeName = "pbkdf2_test_$PID.exe"       # 每进程唯一（$PID = 当前 pwsh 进程号）
$exeRel  = "tests/$exeName"             # 传给 g++ 的相对路径（不引入非 ASCII 绝对路径）
$exe     = Join-Path $testsDir $exeName # 供 Test-Path / 执行 / 清理使用

if (-not (Test-Path -LiteralPath $src)) {
    Write-Host "ERROR: 找不到测试源文件 $src"
    exit 3
}

# ---- 定位 g++ -------------------------------------------------------------
$gxx = $null
$candidates = @()
if ($env:DSH_GXX) { $candidates += $env:DSH_GXX }
$candidates += 'C:\mingw64\bin\g++.exe'
$gxxCmd = Get-Command g++ -ErrorAction SilentlyContinue
if ($gxxCmd) { $candidates += $gxxCmd.Source }
foreach ($c in $candidates) {
    if ($c -and (Test-Path -LiteralPath $c)) { $gxx = $c; break }
}
if (-not $gxx) {
    Write-Host 'ERROR: 找不到 g++（已尝试 $env:DSH_GXX、C:\mingw64\bin\g++.exe、PATH）'
    exit 3
}

# ---- 编译 ----------------------------------------------------------------
$compileArgs = @(
    'tests/pbkdf2_rfc7914_test.cpp',
    '-o', $exeRel,
    '-std=c++17', '-O2', '-I.'
)
Write-Host ">>> $gxx $($compileArgs -join ' ')    (cwd=$repoRoot)"
Push-Location $repoRoot
try {
    & $gxx @compileArgs
    $compileCode = $LASTEXITCODE
} finally {
    Pop-Location
}
Write-Host "<<< g++ exit code = $compileCode"
if ($null -eq $compileCode) { $compileCode = 3 }
if ($compileCode -ne 0) {
    Write-Host ''
    Write-Host 'RESULT: COMPILE FAILED —— 测试目标未生成，后续步骤不再执行（不静默跳过）'
    # 安全修复 T34：编译失败时清理本次唯一产物，避免残留半成品
    if (Test-Path -LiteralPath $exe) { Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue }
    exit 2
}
if (-not (Test-Path -LiteralPath $exe)) {
    Write-Host "ERROR: 编译返回 0 但未生成 $exe"
    exit 3
}

# ---- 运行 ----------------------------------------------------------------
$runArgs = @()
if ($Only) { $runArgs += @('--only', $Only) }
if ($List) { $runArgs += '--list' }

Write-Host ''
Write-Host ">>> $exe $($runArgs -join ' ')"
& $exe @runArgs
$runCode = $LASTEXITCODE
Write-Host "<<< test exit code = $runCode"
if ($null -eq $runCode) {
    Write-Host 'ERROR: 测试进程未返回退出码'
    exit 3
}

if ($runCode -ne 0) {
    Write-Host ''
    Write-Host 'RESULT: FAIL'
    if (-not $Only) {
        Write-Host '提示：若 sha256.h 尚未按 RFC 8018 修复（HMAC key 必须取 password），'
        Write-Host '      此处的失败是**预期的基线失败** —— 见 docs/audit/BATCH0_CONTRACT.md'
        Write-Host '      §3.1 V1-9（反例门禁：修复前必须真实失败）。T4 修复后本脚本应退出 0。'
    }
}

# 安全修复 T34：本次唯一产物用完即删（本身已不会与他人冲突，删除只为不让 tests/ 累积）
if (Test-Path -LiteralPath $exe) { Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue }

exit $runCode
