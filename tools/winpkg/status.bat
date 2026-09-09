@echo off
rem =====================================================================
rem  idealworld 152 server - WHAT IS RUNNING / WHAT IS LISTENING
rem =====================================================================
setlocal EnableExtensions

echo.
echo  processes:
echo  ----------
tasklist /FI "IMAGENAME eq gauthd.exe"      /FO TABLE /NH 2>nul
tasklist /FI "IMAGENAME eq gamedbd.exe"     /FO TABLE /NH 2>nul
tasklist /FI "IMAGENAME eq uniquenamed.exe" /FO TABLE /NH 2>nul
tasklist /FI "IMAGENAME eq gfactiond.exe"   /FO TABLE /NH 2>nul
tasklist /FI "IMAGENAME eq gdeliveryd.exe"  /FO TABLE /NH 2>nul
tasklist /FI "IMAGENAME eq glinkd.exe"      /FO TABLE /NH 2>nul
tasklist /FI "IMAGENAME eq logservice.exe"  /FO TABLE /NH 2>nul
tasklist /FI "IMAGENAME eq gs.exe"          /FO TABLE /NH 2>nul
echo.

echo  listening ports:
echo  ----------------
echo    9001          glinkd      ^<- the game client connects here
echo    29100         gdeliveryd  GDeliveryServer
echo    29200         gauthd      auth
echo    29300/29301   gdeliveryd/glinkd  GProviderServer (gs connects here)
echo    29400         gamedbd     role database
echo    29401         uniquenamed unique names
echo    29500         gfactiond   factions
echo    11100/11101   logservice  UDP/TCP log sink
echo.
netstat -ano -p tcp | findstr "LISTENING" | findstr ":9001 :29100 :29200 :29300 :29301 :29400 :29401 :29500 :11101"
echo.
echo  (a port missing from the list above = that daemon is not running;
echo   look at its window for the error it printed)
echo.
pause
