=====================================================================
 idealworld 152 - WINDOWS SERVER  -  READ THIS FIRST
 (in a hurry?  HOWTO-PLAY.TXT is the 3-step version)
=====================================================================

WHAT IS IN THIS ZIP
  All 8 server programs, compiled from source, already configured to
  talk to each other on 127.0.0.1 (everything on this one PC):

    START-ALL.BAT     starts the server          <- double-click this
    START-GS.BAT      starts the game world (gs)
    STOP-ALL.BAT      stops everything
    STATUS.BAT        shows processes + ports
    HOWTO-PLAY.TXT    the short guide
    README-FIRST.TXT  this file

    gauthd\        gauthd.exe       + gauthd.conf        (login/auth)
    gamedbd\       gamedbd.exe      + gamesys.conf       (characters DB)
    uniquenamed\   uniquenamed.exe  + uniquenamed.conf   (unique names)
    gfaction\      gfactiond.exe    + gamesys.conf       (factions)
    gdeliveryd\    gdeliveryd.exe   + gamesys.conf       (dispatcher)
    glinkd\        glinkd.exe       + gamesys.conf       (client gateway)
    logservice\    logservice.exe   + logservice.conf    (logs)
    gs\            gs.exe           + gs.conf            (the game world)
    *.dll          the runtime libraries the .exe files need

  Every daemon reads its config from its OWN folder - do not move the
  .exe files out of their folders, and keep the .dll files in this
  folder (START-ALL.BAT puts this folder on the DLL search path).


---------------------------------------------------------------------
 IS IT "CLICK AND RUN"?   ... almost.  Two things are NOT in this zip:
---------------------------------------------------------------------

 1) THE GAME DATA  (needed by the game world only)

    Copy your PW 1.5.2 server data package into    gs\data\
    so that at least these exist:

        gs\data\elements.data        gs\data\tasks.data
        gs\data\dyn_tasks.data       gs\data\world_targets.sev
        gs\data\aipolicy.data        gs\data\npcgen.data
        gs\data\precinct.sev         gs\data\region.sev
        gs\data\path.sev
        gs\data\movemap\  gs\data\watermap\  gs\data\airmap\
        gs\data\world\map\   (and b01\ a01\ a02\ a05\ a06\ a07\ if you
                              use the arenas/instances)

    Why it is missing: this data is not part of the server source and is
    not ours to redistribute.  It comes from a PW 1.5.2 era server data
    package, or extracted from a matching game client.

    Without it: the 7 network daemons run perfectly, but gs (the world)
    cannot start.  START-ALL.BAT detects this, starts everything else
    and tells you.

 2) A GAME CLIENT

    A PW 1.5.2 era game client, pointed at this PC on port 9001
    (patch its server list the usual way for that client generation).

    Logging in: use ANY account name and ANY password.  This build
    accepts every login and marks it as a GM - no account server, no
    database of users, no billing service is needed.

  Everything else (configs, databases, log folders, the little files the
  daemons expect in their working directory) is already set up for you.


---------------------------------------------------------------------
 HOW TO RUN IT
---------------------------------------------------------------------

  1. Unpack the zip anywhere, e.g.  C:\pw\
     (a path without spaces and without non-ASCII letters is safest)

  2. Double-click  START-ALL.BAT
        -> 7 windows open, one per daemon.  Leave them open.
        -> if the game data is in place it also starts the world.

  3. Put the game data into  gs\data\   (see above), then
     double-click  START-GS.BAT

  4. Double-click  STATUS.BAT  to check.  You want to see
        9001   listening   (clients connect here)
        29100 29200 29300 29301 29400 29401 29500

  5. Start the game client, connect to  <this PC's IP> : 9001,
     log in with any name/password.

  6. Done?  Double-click  STOP-ALL.BAT

  Other players on your LAN: allow TCP 9001 through the Windows
  firewall (and the 29xxx ports only if you split the daemons over
  several machines).


---------------------------------------------------------------------
 IF SOMETHING DOES NOT WORK
---------------------------------------------------------------------

  A window flashes and closes / a daemon is missing in STATUS.BAT
     Read the message that window printed - every daemon prints its
     config error before it exits.  Most common causes:
       * a .dll is missing  -> keep the .dll files in this folder and
         start the server with START-ALL.BAT (it sets the DLL path)
       * the port is already in use -> run STATUS.BAT, close the other
         copy of the server
       * you moved an .exe out of its folder -> configs use relative
         paths, so the daemon cannot find its config/database

  The world (gs) exits right away
     The game data in gs\data is incomplete.  gs prints which file it
     could not load.

  I want to run a daemon by hand
     Open a cmd window in that daemon's folder and run it there, e.g.
        cd /d C:\pw\glinkd
        set PATH=C:\pw;%PATH%
        glinkd.exe gamesys.conf 1
     Arguments: glinkd takes the section number (1 = [GLinkServer1]),
     gs takes the world name (gs01, arena01, ...).  Windows has no
     fork(), so one world = one gs.exe process.

  More than one world
     START-GS.BAT arena01   (and so on).  Note: worlds on the same PC
     exchange messages over UNIX domain sockets, which Windows does not
     provide here - a single world works normally, several worlds on one
     PC cannot talk to each other.

  Logs
     Each daemon prints to its own window.  If you started them with
     pwctl.sh (MSYS2) instead, the logs are in  logs\<name>.log

---------------------------------------------------------------------
 build info: see MANIFEST.TXT and PACKAGE.TXT in this folder.  Configs
 were patched for a single host (all addresses 127.0.0.1, gauthd port
 29200, no AU certificate authority).  Full technical details:
 RUNBOOK.MD, section "0b. Running on Windows".
=====================================================================
