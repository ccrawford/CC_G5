@echo off
REM Dual G5 Configuration Creator - Batch Launcher
REM This batch file runs the Python script to create a dual G5 MobiFlight configuration

echo.
echo ======================================================================
echo                   Dual G5 Configuration Creator
echo ======================================================================
echo.

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3 from https://www.python.org/
    pause
    exit /b 1
)

REM Run the Python script
python "%~dp0create_dual_g5_config.py"

REM Check if the script succeeded
if errorlevel 1 (
    echo.
    echo ERROR: Script failed with error code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ======================================================================
echo Press any key to exit...
pause >nul
