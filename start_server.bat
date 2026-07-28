@echo off
chcp 65001 >nul 2>&1
REM ========================================
REM 校园管理系统 - 服务器启动脚本（带异常重启）
REM 用法：双击运行或通过 Windows 任务计划程序开机自启
REM ========================================

cd /d "%~dp0"

:loop
echo [%date% %time%] 启动服务器...
start /b /wait server.exe
echo.
echo [%date% %time%] 服务器已停止，退出码: %errorlevel%
echo [%date% %time%] 5秒后自动重启...
timeout /t 5 /nobreak >nul
goto loop
