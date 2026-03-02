@echo off
REM PGPEMU WEB V2 - Complete Installation
REM Features: Captive Portal + Timer Pause + Info Box + All V1 Features

setlocal enabledelayedexpansion

echo.
echo =========================================
echo PGPEMU WEB V2 - COMPLETE INSTALLATION
echo =========================================
echo.
echo NEW FEATURES:
echo   [+] Captive Portal (auto-open browser)
echo   [+] Timer Pause/Resume button
echo   [+] Quick Start info box
echo   [+] All V1 features maintained
echo.
echo FILES TO INSTALL:
echo   - stats.h + stats.c (stats getter)
echo   - wifi_ap_manager.h + .c (with timer control)
echo   - web_server.c (V2 with captive portal)
echo.

if not exist "pgpemu-esp32\main" (
    echo [FEHLER] pgpemu-esp32\main nicht gefunden!
    pause
    exit /b 1
)

echo [OK] Projekt gefunden
echo.

REM Check files
set MISSING=0

if not exist "stats.h" (
    echo [FEHLER] stats.h nicht gefunden!
    set MISSING=1
)

if not exist "stats.c" (
    echo [FEHLER] stats.c nicht gefunden!
    set MISSING=1
)

if not exist "wifi_ap_manager.h" (
    echo [FEHLER] wifi_ap_manager.h nicht gefunden!
    set MISSING=1
)

if not exist "wifi_ap_manager_PAUSABLE.c" (
    echo [FEHLER] wifi_ap_manager_PAUSABLE.c nicht gefunden!
    set MISSING=1
)

if not exist "web_server_V2_CAPTIVE.c" (
    echo [FEHLER] web_server_V2_CAPTIVE.c nicht gefunden!
    set MISSING=1
)

if %MISSING%==1 (
    echo.
    echo Bitte stelle sicher dass alle V2-Dateien heruntergeladen sind.
    pause
    exit /b 1
)

echo [OK] Alle Dateien gefunden
echo.

REM Backup
echo Erstelle Backup...
set BACKUP_DIR=backup_v2_%date:~-4,4%%date:~-7,2%%date:~-10,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set BACKUP_DIR=%BACKUP_DIR: =0%
mkdir "%BACKUP_DIR%" 2>nul

copy "pgpemu-esp32\main\stats.h" "%BACKUP_DIR%\" >nul 2>&1
copy "pgpemu-esp32\main\stats.c" "%BACKUP_DIR%\" >nul 2>&1
copy "pgpemu-esp32\main\wifi_ap_manager.h" "%BACKUP_DIR%\" >nul 2>&1
copy "pgpemu-esp32\main\wifi_ap_manager.c" "%BACKUP_DIR%\" >nul 2>&1
copy "pgpemu-esp32\main\web_server.c" "%BACKUP_DIR%\" >nul 2>&1

echo [OK] Backup: %BACKUP_DIR%
echo.

REM Install
echo Installiere V2 Dateien...

echo [1/5] stats.h
copy "stats.h" "pgpemu-esp32\main\stats.h" >nul 2>&1
if errorlevel 1 (
    echo       [FEHLER] Konnte stats.h nicht kopieren!
    pause
    exit /b 1
)
echo       [OK] stats.h

echo [2/5] stats.c
copy "stats.c" "pgpemu-esp32\main\stats.c" >nul 2>&1
if errorlevel 1 (
    echo       [FEHLER] Konnte stats.c nicht kopieren!
    pause
    exit /b 1
)
echo       [OK] stats.c

echo [3/5] wifi_ap_manager.h
copy "wifi_ap_manager.h" "pgpemu-esp32\main\wifi_ap_manager.h" >nul 2>&1
if errorlevel 1 (
    echo       [FEHLER] Konnte wifi_ap_manager.h nicht kopieren!
    pause
    exit /b 1
)
echo       [OK] wifi_ap_manager.h

echo [4/5] wifi_ap_manager.c
copy "wifi_ap_manager_PAUSABLE.c" "pgpemu-esp32\main\wifi_ap_manager.c" >nul 2>&1
if errorlevel 1 (
    echo       [FEHLER] Konnte wifi_ap_manager.c nicht kopieren!
    pause
    exit /b 1
)
echo       [OK] wifi_ap_manager.c (mit Timer Pause/Resume)

echo [5/5] web_server.c
copy "web_server_V2_CAPTIVE.c" "pgpemu-esp32\main\web_server.c" >nul 2>&1
if errorlevel 1 (
    echo       [FEHLER] Konnte web_server.c nicht kopieren!
    pause
    exit /b 1
)
echo       [OK] web_server.c (V2 mit Captive Portal)

echo.
echo =========================================
echo INSTALLATION ERFOLGREICH!
echo =========================================
echo.
echo NEUE FEATURES V2:
echo.
echo   1. CAPTIVE PORTAL
echo      - Browser oeffnet automatisch
echo      - Keine IP-Adresse eingeben!
echo.
echo   2. TIMER PAUSE/RESUME
echo      - "Pause" Button stoppt Countdown
echo      - "Resume" setzt fort
echo      - Flexibel fuer lange Configs
echo.
echo   3. INFO BOX
echo      - Quick Start Guide auf Seite
echo      - "Button 1 Sekunde halten"
echo      - "WiFi 3 Minuten aktiv"
echo.
echo   4. ALLE V1 FEATURES
echo      - 3 Tabs (Settings/Stats/Devices)
echo      - Statistics (Caught/Fled/Spin)
echo      - Battery Status
echo      - Per-Device Settings
echo.
echo API ENDPOINTS: 10 (vorher 8)
echo   - GET  /api/timer/pause  [NEU]
echo   - POST /api/timer/resume [NEU]
echo.
echo Naechste Schritte:
echo   1. cd pgpemu-esp32
echo   2. idf.py build
echo   3. idf.py flash monitor
echo.
echo Nach dem Flash:
echo   1. Button 1 Sekunde halten
echo   2. WiFi "PGPemu-Setup" verbinden
echo   3. Browser oeffnet AUTOMATISCH!
echo   4. Timer mit Pause/Resume kontrollieren
echo.

set /p BUILD_NOW="Jetzt Build ^& Flash starten? (J/N): "
if /i "%BUILD_NOW%"=="J" (
    echo.
    echo Starte Build ^& Flash...
    cd pgpemu-esp32
    
    echo.
    echo Build...
    call idf.py build
    
    if errorlevel 1 (
        echo.
        echo [FEHLER] Build fehlgeschlagen!
        cd ..
        pause
        exit /b 1
    )
    
    echo.
    echo Flash...
    call idf.py flash monitor
    
    cd ..
)

echo.
echo Details siehe: V2_COMPLETE_GUIDE.md
echo Whitelist-Info siehe: WHITELIST_GUIDE.md
echo.
pause
