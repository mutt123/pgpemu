@echo off
REM WEB SERVER V3 - Android Captive Portal Fix
REM Quick Install Script

echo.
echo ==========================================
echo WEB SERVER V3 - ANDROID CAPTIVE FIX
echo ==========================================
echo.
echo This fixes Android Captive Portal detection!
echo.
echo Changes in V3:
echo   [+] /generate_204 handler (Android)
echo   [+] /gen_204 handler (Android)
echo   [+] /hotspot-detect.html handler
echo   [+] max_open_sockets increased to 15
echo   [+] Better handler ordering
echo.

if not exist "pgpemu-esp32\main" (
    echo [ERROR] pgpemu-esp32\main not found!
    pause
    exit /b 1
)

if not exist "web_server_V3_ANDROID.c" (
    echo [ERROR] web_server_V3_ANDROID.c not found!
    pause
    exit /b 1
)

echo [OK] Files found
echo.

REM Backup
echo Creating backup...
set BACKUP_FILE=web_server_backup_%date:~-4,4%%date:~-7,2%%date:~-10,2%_%time:~0,2%%time:~3,2%%time:~6,2%.c
set BACKUP_FILE=%BACKUP_FILE: =0%
copy "pgpemu-esp32\main\web_server.c" "%BACKUP_FILE%" >nul 2>&1
echo [OK] Backup: %BACKUP_FILE%
echo.

REM Install V3
echo Installing V3...
copy "web_server_V3_ANDROID.c" "pgpemu-esp32\main\web_server.c" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to copy file!
    pause
    exit /b 1
)

echo [OK] web_server.c updated to V3
echo.
echo ==========================================
echo INSTALLATION SUCCESSFUL!
echo ==========================================
echo.
echo Android Captive Portal is now supported!
echo.
echo New URLs registered:
echo   - /generate_204
echo   - /gen_204
echo   - /hotspot-detect.html
echo.
echo Next steps:
echo   1. cd pgpemu-esp32
echo   2. idf.py build
echo   3. idf.py flash monitor
echo.
echo After flashing:
echo   1. Hold button 1 second
echo   2. Connect WiFi "PGPemu-Setup"
echo   3. Android notification appears!
echo   4. Tap notification
echo   5. Browser opens automatically!
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
echo See ANDROID_CAPTIVE_FIX.md for details
echo.
pause
