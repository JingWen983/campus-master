@echo off
chcp 65001 >nul 2>&1
REM ========================================
REM 校园管理系统 - 开机自启注册脚本
REM 将服务器注册为 Windows 任务计划程序任务
REM 需要管理员权限运行
REM ========================================

echo ========================================
echo 校园管理系统 - 开机自启注册
echo ========================================
echo.

REM 检查管理员权限
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 需要管理员权限运行！
    echo 请右键此脚本，选择"以管理员身份运行"
    pause
    exit /b 1
)

set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

echo [INFO] 脚本目录: %SCRIPT_DIR%
echo [INFO] 正在注册开机自启任务...

REM 创建任务计划：开机时自动启动服务器（带异常重启）
schtasks /create /tn "CampusManagementServer" /tr "\"%SCRIPT_DIR%\start_server.bat\"" /sc onlogon /rl highest /f

if %errorlevel% equ 0 (
    echo.
    echo [成功] 开机自启任务已注册！
    echo [INFO] 任务名称: CampusManagementServer
    echo [INFO] 服务器将在每次登录时自动启动
    echo [INFO] 异常退出后5秒自动重启
    echo.
    echo 卸载自启命令:
    echo   schtasks /delete /tn "CampusManagementServer" /f
) else (
    echo.
    echo [错误] 注册失败，请检查权限
)

echo.
pause
