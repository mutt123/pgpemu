@echo off
REM ============================================
REM PGPEMU v1.1.0 - Quick Install
REM Upgrade from v1.0.0 to v1.1.0
REM ============================================

echo.
echo ============================================
echo PGPEMU v1.1.0 INSTALLATION
echo ============================================
echo.
echo NEW FEATURES in v1.1.0:
echo   [+] LED Indicator (GPIO 8, blue LED)
echo   [+] Button Hold: 1s -^> 2s
echo   [+] WiFi Timeout: 3min -^> 5min
echo   [+] WPA2 Password: "PogoPogo"
echo   [+] TX Power: Configurable (8.5 dBm default)
echo   [+] Secrets Tab: SSID/Password/TX Power
echo.
echo Upgrading from v1.0.0 to v1.1.0...
echo.

if not exist "pgpemu-esp32\main" (
    echo [ERROR] pgpemu-esp32\main not found!
    pause
    exit /b 1
)

REM Check files
set MISSING=0

if not exist "button_input_v1.1.0.c" (
    echo [ERROR] button_input_v1.1.0.c missing!
    set MISSING=1
)

if not exist "wifi_ap_manager_v1.1.0.h" (
    echo [ERROR] wifi_ap_manager_v1.1.0.h missing!
    set MISSING=1
)

if not exist "wifi_ap_manager_v1.1.0.c" (
    echo [ERROR] wifi_ap_manager_v1.1.0.c missing!
    set MISSING=1
)

if not exist "web_server_v1.1.0.c" (
    echo [WARN] web_server_v1.1.0.c missing!
    echo        Will create placeholder...
)

if %MISSING%==1 (
    echo.
    echo Please download v1.1.0 files first!
    pause
    exit /b 1
)

echo [OK] Files found
echo.

REM Backup
echo Creating backup...
set BACKUP_DIR=backup_before_v1.1.0_%date:~-4,4%%date:~-7,2%%date:~-10,2%
mkdir "%BACKUP_DIR%" 2>nul

copy "pgpemu-esp32\main\button_input.c" "%BACKUP_DIR%\" >nul 2>&1
copy "pgpemu-esp32\main\wifi_ap_manager.h" "%BACKUP_DIR%\" >nul 2>&1
copy "pgpemu-esp32\main\wifi_ap_manager.c" "%BACKUP_DIR%\" >nul 2>&1
copy "pgpemu-esp32\main\web_server.c" "%BACKUP_DIR%\" >nul 2>&1

echo [OK] Backup: %BACKUP_DIR%
echo.

REM Install
echo Installing v1.1.0...
echo.

echo [1/4] button_input.c (2s hold)
copy "button_input_v1.1.0.c" "pgpemu-esp32\main\button_input.c" >nul 2>&1
if errorlevel 1 (
    echo       [ERROR] Failed!
    pause
    exit /b 1
)
echo       [OK] 2 second button hold enabled

echo [2/4] wifi_ap_manager.h
copy "wifi_ap_manager_v1.1.0.h" "pgpemu-esp32\main\wifi_ap_manager.h" >nul 2>&1
if errorlevel 1 (
    echo       [ERROR] Failed!
    pause
    exit /b 1
)
echo       [OK] New API functions added

echo [3/4] wifi_ap_manager.c
copy "wifi_ap_manager_v1.1.0.c" "pgpemu-esp32\main\wifi_ap_manager.c" >nul 2>&1
if errorlevel 1 (
    echo       [ERROR] Failed!
    pause
    exit /b 1
)
echo       [OK] LED + TX Power + NVS + 5min timeout

echo [4/4] web_server.c
if exist "web_server_v1.1.0.c" (
    copy "web_server_v1.1.0.c" "pgpemu-esp32\main\web_server.c" >nul 2>&1
    if errorlevel 1 (
        echo       [ERROR] Failed!
        pause
        exit /b 1
    )
    echo       [OK] 4 tabs + Secrets tab
) else (
    echo       [SKIP] Using existing web_server.c
    echo       [INFO] You need to add Secrets tab manually
)

echo.
echo ============================================
echo INSTALLATION SUCCESSFUL!
echo ============================================
echo.
echo v1.1.0 Features:
echo   [x] Button: Hold 2 seconds (was 1s)
echo   [x] WiFi: 5 minutes timeout (was 3min)
echo   [x] LED: Blue LED on GPIO 8 during WiFi AP
echo   [x] Security: WPA2 password "PogoPogo"
echo   [x] TX Power: Configurable 2.0-21.0 dBm
echo   [x] Secrets Tab: Configure SSID/Password/TX
echo.
echo Next Steps:
echo   1. cd pgpemu-esp32
echo   2. idf.py build
echo   3. idf.py flash monitor
echo.
echo After Flashing:
echo   - Hold button 2 seconds (not 1!)
echo   - Blue LED will light up
echo   - Connect with password "PogoPogo"
echo   - WiFi stays on for 5 minutes
echo   - Check "Secrets" tab for config
echo.

set /p BUILD_NOW="Build and flash now? (Y/N): "
if /i "%BUILD_NOW%"=="Y" (
    echo.
    echo Building v1.1.0...
    cd pgpemu-esp32
    
    call idf.py build
    
    if errorlevel 1 (
        echo.
        echo [ERROR] Build failed!
        cd ..
        pause
        exit /b 1
    )
    
    echo.
    echo Flashing...
    call idf.py flash monitor
    
    cd ..
)

echo.
echo See v1.1.0_RELEASE_NOTES.md for details
echo.
pause
