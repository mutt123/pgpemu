@echo off
REM ============================================
REM PGPEMU v1.2.0 - Device Config Installation
REM ============================================

echo.
echo ============================================
echo PGPEMU v1.2.0 - DEVICE CONFIG TAB
echo ============================================
echo.
echo NEW in v1.2.0:
echo   [+] 5th Tab: Device Config
echo   [+] PGP_CLONE_NAME configurable
echo   [+] PGP_MAC configurable
echo   [+] PGP_BLOB configurable
echo   [+] PGP_DEVICE_KEY configurable
echo   [+] Input validation (MAC, Hex)
echo   [+] NVS storage (persistent)
echo   [+] Reset to defaults button
echo.

if not exist "pgpemu-esp32\main" (
    echo [ERROR] pgpemu-esp32\main not found!
    pause
    exit /b 1
)

REM Check files
set MISSING=0

if not exist "device_config.h" (
    echo [ERROR] device_config.h missing!
    set MISSING=1
)

if not exist "device_config.c" (
    echo [ERROR] device_config.c missing!
    set MISSING=1
)

if %MISSING%==1 (
    echo.
    echo Please download v1.2.0 files first!
    pause
    exit /b 1
)

echo [OK] Files found
echo.

REM Backup
echo Creating backup...
set BACKUP_DIR=backup_before_v1.2.0_%date:~-4,4%%date:~-7,2%%date:~-10,2%
mkdir "%BACKUP_DIR%" 2>nul

copy "pgpemu-esp32\main\web_server.c" "%BACKUP_DIR%\" >nul 2>&1
copy "pgpemu-esp32\main\pgpemu.c" "%BACKUP_DIR%\" >nul 2>&1

echo [OK] Backup: %BACKUP_DIR%
echo.

REM Install
echo Installing v1.2.0 files...
echo.

echo [1/2] device_config.h
copy "device_config.h" "pgpemu-esp32\main\device_config.h" >nul 2>&1
if errorlevel 1 (
    echo       [ERROR] Failed!
    pause
    exit /b 1
)
echo       [OK] API header installed

echo [2/2] device_config.c
copy "device_config.c" "pgpemu-esp32\main\device_config.c" >nul 2>&1
if errorlevel 1 (
    echo       [ERROR] Failed!
    pause
    exit /b 1
)
echo       [OK] Implementation installed

echo.
echo ============================================
echo FILES INSTALLED!
echo ============================================
echo.
echo IMPORTANT: Manual steps required!
echo.
echo 1. Edit pgpemu-esp32\main\pgpemu.c
echo    Add at top:
echo      #include "device_config.h"
echo.
echo    Add in app_main(), before wifi_ap_manager_init():
echo      device_config_init();
echo.
echo 2. Edit pgpemu-esp32\main\CMakeLists.txt
echo    Add to SRCS list:
echo      "device_config.c"
echo.
echo 3. Patch web_server.c
echo    See: web_server_v1.2.0_PATCH.md
echo    - Add 5th tab
echo    - Add Device Config content
echo    - Add JavaScript functions
echo    - Add API handlers
echo    - Register URIs
echo.
echo After completing manual steps:
echo   cd pgpemu-esp32
echo   idf.py build flash monitor
echo.

set /p CONTINUE="Open patch documentation now? (Y/N): "
if /i "%CONTINUE%"=="Y" (
    if exist "web_server_v1.2.0_PATCH.md" (
        start notepad "web_server_v1.2.0_PATCH.md"
    ) else (
        echo [WARN] web_server_v1.2.0_PATCH.md not found!
    )
)

echo.
echo ============================================
echo NEXT STEPS CHECKLIST
echo ============================================
echo.
echo Backend:
echo   [x] device_config.h copied
echo   [x] device_config.c copied
echo   [ ] pgpemu.c: Add #include
echo   [ ] pgpemu.c: Add device_config_init()
echo   [ ] CMakeLists.txt: Add device_config.c
echo.
echo Frontend:
echo   [ ] web_server.c: Add 5th tab HTML
echo   [ ] web_server.c: Add Device Config content
echo   [ ] web_server.c: Add JavaScript functions
echo   [ ] web_server.c: Add API handlers
echo   [ ] web_server.c: Register URIs
echo   [ ] web_server.c: Update max_open_sockets to 20
echo.
echo Then:
echo   [ ] cd pgpemu-esp32
echo   [ ] idf.py build
echo   [ ] idf.py flash
echo   [ ] idf.py monitor
echo.
echo Test:
echo   [ ] 5 tabs visible
echo   [ ] Device Config tab opens
echo   [ ] All 4 fields present
echo   [ ] Save button works
echo   [ ] Reset button works
echo   [ ] Settings persist after reboot
echo.
echo See v1.2.0_RELEASE_NOTES.md for details
echo.
pause
