@echo off
REM SECOND START FIX - Quick Install
REM Fixes crash when WiFi AP is started multiple times

echo.
echo ==========================================
echo SECOND START CRASH - FIX
echo ==========================================
echo.
echo This fixes the crash when starting WiFi AP
echo multiple times (2nd, 3rd, 4th time...)
echo.
echo Problem:
echo   - First start: OK
echo   - Second start: CRASH (duplicate netif)
echo.
echo Solution:
echo   - Reuse netif instead of recreating
echo   - Store netif as global variable
echo.

if not exist "pgpemu-esp32\main" (
    echo [ERROR] pgpemu-esp32\main not found!
    pause
    exit /b 1
)

if not exist "wifi_ap_manager_PAUSABLE.c" (
    echo [ERROR] wifi_ap_manager_PAUSABLE.c not found!
    pause
    exit /b 1
)

echo [OK] Files found
echo.

REM Backup
echo Creating backup...
set BACKUP_FILE=wifi_ap_manager_backup_%date:~-4,4%%date:~-7,2%%date:~-10,2%_%time:~0,2%%time:~3,2%%time:~6,2%.c
set BACKUP_FILE=%BACKUP_FILE: =0%
copy "pgpemu-esp32\main\wifi_ap_manager.c" "%BACKUP_FILE%" >nul 2>&1
echo [OK] Backup: %BACKUP_FILE%
echo.

REM Install
echo Installing fix...
copy "wifi_ap_manager_PAUSABLE.c" "pgpemu-esp32\main\wifi_ap_manager.c" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to copy file!
    pause
    exit /b 1
)

echo [OK] wifi_ap_manager.c updated
echo.
echo ==========================================
echo INSTALLATION SUCCESSFUL!
echo ==========================================
echo.
echo Changes:
echo   - netif stored as global variable
echo   - Reused across start/stop cycles
echo   - No crash on 2nd, 3rd, 4th start
echo.
echo Next steps:
echo   1. cd pgpemu-esp32
echo   2. idf.py build
echo   3. idf.py flash monitor
echo.
echo Test:
echo   1. Hold button 1s (start WiFi)
echo   2. Wait 3 min or stop manually
echo   3. Hold button 1s AGAIN
echo   4. WiFi starts without crash!
echo.

set /p BUILD_NOW="Build and flash now? (Y/N): "
if /i "%BUILD_NOW%"=="Y" (
    echo.
    echo Starting build...
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
echo See SECOND_START_FIX.md for details
echo.
pause
