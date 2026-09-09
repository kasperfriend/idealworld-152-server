@echo off
rem =====================================================================
rem  idealworld 152 server - START THE GAME WORLD (gs)
rem
rem    START-GS.BAT            starts world gs01
rem    START-GS.BAT arena01    starts a different world (one window each;
rem                            Windows has no fork, so every world is its
rem                            own gs.exe process)
rem =====================================================================
setlocal EnableExtensions
set "ROOT=%~dp0"
set "PATH=%ROOT%;%PATH%"
set "WORLD=%~1"
if "%WORLD%"=="" set "WORLD=gs01"

if not exist "%ROOT%gs\data\elements.data" (
    echo.
    echo  [!!] %ROOT%gs\data\elements.data not found.
    echo       The game world cannot start without the game data package.
    echo       Copy it into  %ROOT%gs\data  and run this file again.
    echo       See HOWTO-PLAY.TXT, section "1) WHAT TO ADD, AND WHERE".
    echo.
    pause
    exit /b 1
)

cd /d "%ROOT%gs"
start "gs-%WORLD%" /D "%ROOT%gs" cmd /k gs.exe %WORLD%
echo  [ok] game world %WORLD% started
timeout /t 3 /nobreak >nul
