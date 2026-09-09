@echo off
rem =====================================================================
rem  idealworld 152 server - START EVERYTHING
rem  One window per daemon.  Leave the windows open while you play.
rem  Stop again with STOP-ALL.BAT.   See HOWTO-PLAY.TXT / README-FIRST.TXT.
rem =====================================================================
setlocal EnableExtensions
set "ROOT=%~dp0"
rem the mingw runtime DLLs live next to this file; put them on the search path
set "PATH=%ROOT%;%PATH%"
cd /d "%ROOT%"

echo.
echo  idealworld 152 server - starting
echo  folder: %ROOT%
echo.

start "gauthd"      /D "%ROOT%gauthd"      cmd /k gauthd.exe gauthd.conf
timeout /t 2 /nobreak >nul
start "gamedbd"     /D "%ROOT%gamedbd"     cmd /k gamedbd.exe gamesys.conf
start "uniquenamed" /D "%ROOT%uniquenamed" cmd /k uniquenamed.exe uniquenamed.conf
start "gfactiond"   /D "%ROOT%gfaction"    cmd /k gfactiond.exe gamesys.conf
timeout /t 2 /nobreak >nul
start "gdeliveryd"  /D "%ROOT%gdeliveryd"  cmd /k gdeliveryd.exe gamesys.conf
start "logservice"  /D "%ROOT%logservice"  cmd /k logservice.exe logservice.conf
timeout /t 2 /nobreak >nul
start "glinkd"      /D "%ROOT%glinkd"      cmd /k glinkd.exe gamesys.conf 1

echo  [ok] gauthd gamedbd uniquenamed gfactiond gdeliveryd logservice glinkd
echo.

if exist "%ROOT%gs\data\elements.data" (
    start "gs-gs01" /D "%ROOT%gs" cmd /k gs.exe gs01
    echo  [ok] game world gs01
) else (
    echo  [!!] game world gs01 NOT started - game data is missing.
    echo       copy your PW 1.5.2 data package into   %ROOT%gs\data
    echo       then double-click START-GS.BAT
    echo       ^(the 7 daemons above are running; only the world is missing^)
)

echo.
echo  clients connect to  this-pc-ip : 9001
echo  what is listening?  double-click STATUS.BAT
echo.
pause
