@echo off
rem =====================================================================
rem  idealworld 152 server - STOP EVERYTHING
rem
rem  Windows cannot deliver the SIGUSR1 "checkpoint and quit" signal that
rem  the Linux shutdown uses, so this simply ends the processes.  The
rem  embedded databases (gamedbd\dbhomewdb, uniquenamed\uname) replay
rem  their logs the next time they start.
rem =====================================================================
setlocal EnableExtensions

for %%P in (gs.exe glinkd.exe gdeliveryd.exe gfactiond.exe logservice.exe uniquenamed.exe gamedbd.exe gauthd.exe) do (
    tasklist /FI "IMAGENAME eq %%P" 2>nul | find /I "%%P" >nul && (
        taskkill /IM %%P /F >nul 2>&1
        echo  [ok] stopped %%P
    )
)

echo.
echo  all daemons stopped - you can close the leftover windows.
timeout /t 3 /nobreak >nul
