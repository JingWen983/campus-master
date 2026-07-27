@echo off
chcp 65001 >nul 2>&1
echo ========================================
echo Campus Management System - Build Script
echo ========================================
echo.

REM ============================================================
REM Phase 1: 前端构建（Vite + Vue SFC → frontend/dist）
REM ============================================================
echo [INFO] Building frontend (Vite + Vue SFC)...

REM 检查 Node.js / npm
where npm >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Node.js / npm not found in PATH.
    echo         Please install Node.js 18+ from https://nodejs.org/
    pause
    exit /b 1
)

REM 检查 frontend 目录
if not exist "frontend\package.json" (
    echo [ERROR] frontend\package.json not found.
    pause
    exit /b 1
)

pushd frontend

REM 首次构建或依赖缺失时执行 npm install
if not exist "node_modules" (
    echo [INFO] Installing frontend dependencies (npm install)...
    call npm install
    if %errorlevel% neq 0 (
        echo [ERROR] npm install failed!
        popd
        pause
        exit /b 1
    )
)

echo [INFO] Running Vite build...
call npm run build
if %errorlevel% neq 0 (
    echo [ERROR] Frontend build failed!
    popd
    pause
    exit /b 1
)

popd

REM 校验构建产物
if not exist "frontend\dist\index.html" (
    echo [ERROR] frontend\dist\index.html not found after build.
    pause
    exit /b 1
)
echo [SUCCESS] Frontend build complete -^> frontend\dist\
echo.

REM ============================================================
REM Phase 2: C++ 后端编译（SQLite 模式）
REM ============================================================
echo [INFO] Building SQLite database mode...

REM Check for SQLite files
if not exist "sqlite3.h" (
    echo [WARNING] sqlite3.h not found
    echo.
    echo Please download SQLite amalgamation package.
    echo.
    pause
    exit /b 1
)

if not exist "sqlite3.dll" (
    echo [WARNING] sqlite3.dll not found
    echo Please download sqlite-dll-win64-*.zip from sqlite.org
    echo.
    pause
    exit /b 1
)

echo [INFO] Compiling SQLite C library...
gcc -c sqlite3.c -o sqlite3.o -O2
if %errorlevel% neq 0 (
    echo [ERROR] SQLite C compilation failed!
    pause
    exit /b 1
)

echo [INFO] Compiling C++ source files...
g++ -c main.cpp -o main.o -std=c++11 -O2 -I.
if %errorlevel% neq 0 (
    echo [ERROR] main.cpp compilation failed!
    pause
    exit /b 1
)

g++ -c models.cpp -o models.o -std=c++11 -O2 -I.
if %errorlevel% neq 0 (
    echo [ERROR] models.cpp compilation failed!
    pause
    exit /b 1
)

g++ -c logger.cpp -o logger.o -std=c++11 -O2 -I.
if %errorlevel% neq 0 (
    echo [ERROR] logger.cpp compilation failed!
    pause
    exit /b 1
)

g++ -c routes_static.cpp -o routes_static.o -std=c++11 -O2 -I.
if %errorlevel% neq 0 (
    echo [ERROR] routes_static.cpp compilation failed!
    pause
    exit /b 1
)

g++ -c routes_public.cpp -o routes_public.o -std=c++11 -O2 -I.
if %errorlevel% neq 0 (
    echo [ERROR] routes_public.cpp compilation failed!
    pause
    exit /b 1
)

g++ -c routes_admin.cpp -o routes_admin.o -std=c++11 -O2 -I.
if %errorlevel% neq 0 (
    echo [ERROR] routes_admin.cpp compilation failed!
    pause
    exit /b 1
)

g++ -c routes_teacher.cpp -o routes_teacher.o -std=c++11 -O2 -I.
if %errorlevel% neq 0 (
    echo [ERROR] routes_teacher.cpp compilation failed!
    pause
    exit /b 1
)

g++ -c routes_student.cpp -o routes_student.o -std=c++11 -O2 -I.
if %errorlevel% neq 0 (
    echo [ERROR] routes_student.cpp compilation failed!
    pause
    exit /b 1
)

g++ -c routes_parent.cpp -o routes_parent.o -std=c++11 -O2 -I.
if %errorlevel% neq 0 (
    echo [ERROR] routes_parent.cpp compilation failed!
    pause
    exit /b 1
)

echo [INFO] Linking (静态链接 MinGW 运行时，避免 DLL 依赖)...
g++ -o server.exe main.o models.o logger.o routes_static.o routes_public.o routes_admin.o routes_teacher.o routes_student.o routes_parent.o sqlite3.o -lws2_32 -lwsock32 -std=c++11 -O2 -static -static-libgcc -static-libstdc++ -lwinpthread
if %errorlevel% neq 0 (
    echo [ERROR] Linking failed!
    pause
    exit /b 1
)

echo [SUCCESS] Build complete (SQLite Database Mode)
echo [INFO] Data will be persisted to campus_system.db
echo.
echo ========================================
echo Build complete!
echo ========================================
echo.
echo Default accounts:
echo   Admin:   admin / admin123
echo   Teacher: teacher / teacher123
echo   Student: student / student123
echo.
pause
