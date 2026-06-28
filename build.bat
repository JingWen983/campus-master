@echo off
chcp 65001 >nul 2>&1
echo ========================================
echo Campus Management System - Build Script
echo ========================================
echo.

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

echo [INFO] Linking...
g++ -o server.exe main.o models.o logger.o routes_static.o routes_public.o routes_admin.o routes_teacher.o routes_student.o routes_parent.o sqlite3.o -lws2_32 -lwsock32 -std=c++11 -O2
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
