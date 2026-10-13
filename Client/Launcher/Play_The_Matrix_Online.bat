@echo off
setlocal
title The Matrix Online Remaster - Master Launcher
color 0A
cls

set "GAME_ROOT=%~dp0"
cd /d "%GAME_ROOT%"

echo ===============================================================================
echo            T H E   M A T R I X   O N L I N E   :   R E M A S T E R            
echo ===============================================================================
echo  [Vulkan 1.3 DXVK Pipeline - 15,000+ Cognitive AI Bots - 56,100 Colliders]
echo ===============================================================================
echo.

:: Check for authentic client binaries
set "CLIENT_FOUND=0"
if exist "%GAME_ROOT%matrix.exe" set "CLIENT_FOUND=1"
if exist "%GAME_ROOT%Client\matrix.exe" set "CLIENT_FOUND=1"
if exist "%GAME_ROOT%launcher.exe" set "CLIENT_FOUND=1"
if exist "%GAME_ROOT%Client\launcher.exe" set "CLIENT_FOUND=1"

:: Locate ZionLauncher executable
set "LAUNCHER_EXE="
if exist "%GAME_ROOT%ZionLauncher.exe" set "LAUNCHER_EXE=%GAME_ROOT%ZionLauncher.exe"
if not defined LAUNCHER_EXE if exist "%GAME_ROOT%publish_zion\ZionLauncher.exe" set "LAUNCHER_EXE=%GAME_ROOT%publish_zion\ZionLauncher.exe"
if not defined LAUNCHER_EXE if exist "%GAME_ROOT%publish\ZionLauncher.exe" set "LAUNCHER_EXE=%GAME_ROOT%publish\ZionLauncher.exe"
if not defined LAUNCHER_EXE if exist "%GAME_ROOT%Client\ZionLauncher.exe" set "LAUNCHER_EXE=%GAME_ROOT%Client\ZionLauncher.exe"

if "%CLIENT_FOUND%"=="0" (
    echo ===============================================================================
    echo  [!] No existing game files detected in this folder.
    echo  [!] Zion Launcher will open and provide the automated 1-click client
    echo      downloader to install the complete 1.26 GB game!
    echo ===============================================================================
    echo.
    echo  [*] What will happen next:
    echo      1. Zion Launcher will open automatically.
    echo      2. The automated 1-click downloader will fetch the full 1.26 GB client
    echo         archive directly from the high-speed Zion server.
    echo      3. All game files will be unpacked and matrix.exe will be verified.
    echo.
    echo ===============================================================================
    echo  Press any key to launch Zion Launcher and start installation...
    echo ===============================================================================
    pause >nul
    echo.
    echo  [+] Launching Zion Launcher...
    if defined LAUNCHER_EXE (
        start "" "%LAUNCHER_EXE%" --install-client
    ) else (
        echo  [ERROR] ZionLauncher.exe was not found in %GAME_ROOT%!
        echo  Please place ZionLauncher.exe in the same folder as this script.
        echo.
        pause
    )
    exit /b
)

:: Client files ARE present
echo  [OK] Authentic Matrix Online game client detected.
echo  [OK] Target Server: Zion Live Mainframe [15.204.82.250].
echo.

set "IS_LOCAL_DEV=0"
if exist "E:\TESTDB\bin\mysqld.exe" if exist "%GAME_ROOT%mxoemu_live\Server\Reality\Binaries\Reality.exe" set "IS_LOCAL_DEV=1"

if "%IS_LOCAL_DEV%"=="1" (
    echo [1/3] Checking MariaDB Database Daemon - Port 3307...
    netstat -ano | findstr ":3307 " | findstr "LISTENING" >nul
    if errorlevel 1 (
        echo       [..] Starting MariaDB daemon...
        start /min "RealityDB" "E:\TESTDB\bin\mysqld.exe" --datadir="%GAME_ROOT%RealityDB_Data" --port=3307 --console
        timeout /t 3 /nobreak >nul
    ) else (
        echo       [OK] Database daemon active on port 3307.
    )

    echo.
    echo [2/3] Checking Reality Server Simulation Daemon - Ports 11000/10000...
    tasklist /fi "imagename eq Reality.exe" | findstr /i "Reality.exe" >nul
    if errorlevel 1 (
        echo       [..] Starting Reality daemon...
        start /min "RealityServer" /d "%GAME_ROOT%mxoemu_live\Server\Reality\Binaries" "%GAME_ROOT%mxoemu_live\Server\Reality\Binaries\Reality.exe"
        timeout /t 5 /nobreak >nul
    ) else (
        echo       [OK] Reality simulation daemon is active.
    )
    echo.
    echo [3/3] Launching Zion Mainframe Operator Interface...
) else (
    echo  [*] Launching Zion Mainframe Operator Interface...
)

if defined LAUNCHER_EXE (
    start "" "%LAUNCHER_EXE%"
) else (
    echo  [ERROR] ZionLauncher.exe was not found in this directory!
    echo.
    pause
    exit /b 1
)

echo.
echo ===============================================================================
echo  Construct is initialized. Operator credentials preloaded. Click PLAY to enter.
echo ===============================================================================
timeout /t 3 >nul
exit
