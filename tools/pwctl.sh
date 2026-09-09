#!/usr/bin/env bash
#
# pwctl.sh - one-click launcher for the idealworld-152 server runtime tree.
#
# Expects the layout created by tools/setup-runtime.sh (this script sits at the
# runtime root, one directory per daemon next to it):
#
#   .
#   ├── pwctl.sh          <- this file
#   ├── gauthd/           gauthd + gauthd.conf          + dbhome/
#   ├── gdeliveryd/       gdeliveryd + gamesys.conf
#   ├── glinkd/           glinkd + gamesys.conf         + dbhome/
#   ├── gfaction/         gfactiond + gamesys.conf      + dbhome/
#   ├── gamedbd/          gamedbd + gamesys.conf        + dbhomewdb/
#   ├── uniquenamed/      uniquenamed + uniquenamed.conf + uname/
#   ├── logservice/       logservice + logservice.conf  + logdata/
#   ├── gs/               gs + gs.conf + gmserver.conf + gsalias.conf
#   │                     + data/ (elements.data, tasks.data, maps...)
#   ├── gs/worlds.list    which worlds `start` launches (default: gs01)
#   ├── logs/             stdout/stderr of every daemon
#   └── pids/             pidfiles
#
# Usage:
#   ./pwctl.sh                 start everything (the "one click")
#   ./pwctl.sh start [name..]  start all, or only the named daemons/worlds
#   ./pwctl.sh stop  [name..]  stop all (reverse order), or only named
#   ./pwctl.sh restart [name..]
#   ./pwctl.sh status          process + port table
#   ./pwctl.sh logs [name]     tail -f the daemon log (default: gdeliveryd)
#   ./pwctl.sh console <name>  run one daemon in the foreground (debug)
#
# Names: gauthd gdeliveryd glinkd gfactiond gamedbd uniquenamed logservice,
#        and worlds as gs-<world> (e.g. gs-gs01, matching gs.conf sections).
#
# Environment:
#   PW_WORLDS    override worlds.list ("gs01 gs02 ...")
#   PWCTL_FORCE_GS=1   try to start gs even if the data files are missing
#
set -u

PWROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PWROOT"
LOGDIR="$PWROOT/logs"; PIDDIR="$PWROOT/pids"
mkdir -p "$LOGDIR" "$PIDDIR" 2>/dev/null

if [ ! -t 1 ]; then C_RESET=''; C_GREEN=''; C_RED=''; C_YELLOW=''; C_CYAN=''
else C_RESET=$'\e[0m'; C_GREEN=$'\e[32m'; C_RED=$'\e[31m'; C_YELLOW=$'\e[33m'; C_CYAN=$'\e[36m'; fi
ok()   { echo "${C_GREEN}[ok]${C_RESET}  $*"; }
skip() { echo "${C_YELLOW}[skip]${C_RESET} $*"; }
fail() { echo "${C_RED}[FAIL]${C_RESET} $*"; }
info() { echo "${C_CYAN}[i]${C_RESET}   $*"; }

# ---------------------------------------------------------------- daemons ---
# name|dir|command line|tcp port to probe (0 = none)|stop signal
DAEMONS=(
"gauthd|gauthd|./gauthd gauthd.conf|29200|TERM"
"gamedbd|gamedbd|./gamedbd gamesys.conf|29400|USR1"
"uniquenamed|uniquenamed|./uniquenamed uniquenamed.conf|29401|USR1"
"gfactiond|gfaction|./gfactiond gamesys.conf|29500|TERM"
"gdeliveryd|gdeliveryd|./gdeliveryd gamesys.conf|29100|TERM"
"glinkd|glinkd|./glinkd gamesys.conf 1|9001|TERM"
"logservice|logservice|./logservice logservice.conf|11101|TERM"
)
# Start order above: listeners first (auth/db/uniquename/faction), then the
# dispatcher, the client gateway, then auxiliary services.  Every connector
# retries forever, so the order is about clean startup logs, not correctness.

usage() { sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 1; }

daemon_field() { echo "$1" | cut -d"|" -f"$2"; }

pidfile_of() { echo "$PIDDIR/$1.pid"; }

# Windows builds install <daemon>.exe (tools/ci-build-win.sh -> dist-win/bin);
# Linux builds install <daemon>.  Rewrite the executable of a command line to
# the .exe when that is what is present, so the same table works for both.
# No-op on Linux, where the extensionless binary exists.
exe_fix() { # <cmd line> -> <cmd line with .exe if needed>
	local c="$1" bin rest
	bin="${c%% *}"; rest="${c#"$bin"}"
	if [ ! -f "$bin" ] && [ -f "$bin.exe" ]; then bin="$bin.exe"; fi
	echo "$bin$rest"
}

# Daemons fork helpers (e.g. gauthd), so every daemon is started as its own
# session/process-group and the pidfile holds the group id.  "is running"
# and "stop" therefore operate on the whole group.
is_running() { # <name>
	local pf pid; pf="$(pidfile_of "$1")"
	[ -f "$pf" ] || return 1
	pid="$(cat "$pf" 2>/dev/null)"
	[ -n "$pid" ] || return 1
	kill -0 "$pid" 2>/dev/null && return 0     # leader alive
	kill -0 -"$pid" 2>/dev/null && return 0    # any group member alive
	return 1
}

port_open() { # <port> - no connection, just the kernel's table
	local port="$1"
	if command -v ss >/dev/null 2>&1; then
		ss -ltnH "sport = :$port" 2>/dev/null | grep -q .
	else
		timeout 1 bash -c "exec 3<>/dev/tcp/127.0.0.1/$port" 2>/dev/null
	fi
}

tail_log() { # <name> [follow]
	local n="${1:-gdeliveryd}"
	if [ "${2:-}" = "-f" ]; then echo "== follow $LOGDIR/$n.log (Ctrl-C to stop)"; tail -f "$LOGDIR/$n.log"
	else tail -n 40 "$LOGDIR/$n.log" 2>/dev/null || echo "(no log yet: $LOGDIR/$n.log)"; fi
}

# ------------------------------------------------------------ gs worlds -----
worlds() {
	if [ -n "${PW_WORLDS:-}" ]; then echo "$PW_WORLDS"; return; fi
	grep -vE '^[[:space:]]*(#|;|$)' gs/worlds.list 2>/dev/null || echo gs01
}

gs_data_missing() {
	local miss=()
	[ -f gs/data/elements.data ] || miss+=("gs/data/elements.data")
	[ -f gs/data/tasks.data   ] || miss+=("gs/data/tasks.data")
	[ ${#miss[@]} -eq 0 ] && return 1
	echo "${miss[*]}"; return 0
}

# ---------------------------------------------------------------- start -----
start_one() { # <name|dir|cmd|port|sig>  OR  <gs-world>
	local name dir cmd port
	if [ "$1" != "${1#gs-}" ] && [ "$1" != "gs" ]; then        # gs-<world>
		name="$1"; dir=gs; cmd="./gs ${1#gs-}"; port=0
	else
		name="$(daemon_field "$1" 1)"; dir="$(daemon_field "$1" 2)"
		cmd="$(daemon_field "$1" 3)";  port="$(daemon_field "$1" 4)"
	fi

	if is_running "$name"; then ok "$name already running (pid $(cat "$(pidfile_of "$name")"))"; return 0; fi

	local bin; bin="$(echo "$cmd" | awk '{print $1}')"
	if [ ! -x "$dir/$bin" ] && [ ! -x "$dir/$bin.exe" ]; then
		fail "$name: missing executable $dir/$bin (or $bin.exe)"; return 1
	fi

	if [ "$dir" = gs ]; then
		local missing; missing="$(gs_data_missing || true)"
		if [ -n "$missing" ] && [ "${PWCTL_FORCE_GS:-0}" != "1" ]; then
			skip "$name: game data not found ($missing)."
			skip "        drop a PW-152 data package into gs/data/ (see RUNBOOK.md §5) or set PWCTL_FORCE_GS=1."
			return 1
		fi
	fi

	( cd "$dir" && cmd="$(exe_fix "$cmd")" && \
		if command -v setsid >/dev/null 2>&1; then
			setsid nohup $cmd </dev/null >>"$LOGDIR/$name.log" 2>&1 & echo $! >"$PIDDIR/$name.pid"
		else
			nohup $cmd </dev/null >>"$LOGDIR/$name.log" 2>&1 & echo $! >"$PIDDIR/$name.pid"
		fi )

	local i pid; pid="$(cat "$(pidfile_of "$name")" 2>/dev/null)"
	for i in 1 2 3 4 5 6 7 8 9 10; do
		is_running "$name" || break
		[ "$port" != 0 ] && port_open "$port" && break
		sleep 0.5
	done
	if is_running "$name"; then
		if [ "$port" != 0 ] && port_open "$port"; then ok "$name started (pid $pid, port $port listening)"
		else ok "$name started (pid $pid)"; fi
	else
		fail "$name exited immediately - last log lines:"
		tail -n 8 "$LOGDIR/$name.log" 2>/dev/null | sed 's/^/        /'
		rm -f "$(pidfile_of "$name")"
		return 1
	fi
}

cmd_start() {
	local want="${*:2}" rc=0 d w started_any=0
	for d in "${DAEMONS[@]}"; do
		local n; n="$(daemon_field "$d" 1)"
		[ -n "$want" ] && [[ " $want " != *" $n "* ]] && continue
		start_one "$d" || rc=1; started_any=1
	done
	for w in $(worlds); do
		[ -n "$want" ] && [[ " $want " != *" gs-$w "* ]] && continue
		start_one "gs-$w" || rc=1; started_any=1
	done
	[ $started_any -eq 0 ] && fail "nothing matches: $want"
	echo
	info "client entry point : glinkd port $(daemon_field "${DAEMONS[5]}" 4) (version string lives in glinkd/gamesys.conf)"
	info "logs               : $LOGDIR/<name>.log   (./pwctl.sh logs <name>)"
	info "stop               : ./pwctl.sh stop"
	return $rc
}

# ----------------------------------------------------------------- stop -----
stop_one() { # <name> <signal>
	local name="$1" sig="$2" pf pid i
	pf="$(pidfile_of "$name")"
	if ! is_running "$name"; then skip "$name not running"; rm -f "$pf"; return 0; fi
	pid="$(cat "$pf")"
	[ "$sig" = USR1 ] && info "$name: sending SIGUSR1 (clean checkpoint, can take a while)"
	kill -s "$sig" -"$pid" 2>/dev/null || kill -s "$sig" "$pid" 2>/dev/null
	local limit=10; [ "$sig" = USR1 ] && limit=30
	for ((i=0; i<limit*2; i++)); do
		is_running "$name" || { ok "$name stopped"; rm -f "$pf"; return 0; }
		sleep 0.5
	done
	kill -TERM -"$pid" 2>/dev/null; sleep 3
	is_running "$name" || { ok "$name stopped (after TERM)"; rm -f "$pf"; return 0; }
	kill -KILL -"$pid" 2>/dev/null; sleep 1
	is_running "$name" || { ok "$name killed"; rm -f "$pf"; return 0; }
	fail "$name: could not stop pgid $pid"; return 1
}

cmd_stop() {
	local want="${*:2}" rc=0 d w
	# worlds first (they depend on everything), then daemons in reverse
	for w in $(worlds); do
		[ -n "$want" ] && [[ " $want " != *" gs-$w "* ]] && continue
		stop_one "gs-$w" TERM || rc=1
	done
	for ((idx=${#DAEMONS[@]}-1; idx>=0; idx--)); do
		d="${DAEMONS[idx]}"; local n; n="$(daemon_field "$d" 1)"
		[ -n "$want" ] && [[ " $want " != *" $n "* ]] && continue
		stop_one "$n" "$(daemon_field "$d" 5)" || rc=1
	done
	return $rc
}

# --------------------------------------------------------------- status -----
cmd_status() {
	local d n pid port state line
	printf "%-16s %-8s %-10s %s\n" "DAEMON" "PID" "STATE" "PORT"
	printf "%-16s %-8s %-10s %s\n" "------" "---" "-----" "----"
	for d in "${DAEMONS[@]}"; do
		n="$(daemon_field "$d" 1)"; port="$(daemon_field "$d" 4)"
		if is_running "$n"; then
			pid="$(cat "$(pidfile_of "$n")")"; state="${C_GREEN}running${C_RESET}"
			if [ "$port" != 0 ]; then
				port_open "$port" && port="$port open" || port="$port CLOSED"
			else port="-"; fi
		else
			pid="-"; state="${C_RED}stopped${C_RESET}"; port="-"
		fi
		printf "%-16s %-8s %-19s %s\n" "$n" "$pid" "$state" "$port"
	done
	local w
	for w in $(worlds); do
		if is_running "gs-$w"; then
			pid="$(cat "$(pidfile_of "gs-$w")")"
			printf "%-16s %-8s %-19s %s\n" "gs-$w" "$pid" "${C_GREEN}running${C_RESET}" "-"
		else
			printf "%-16s %-8s %-19s %s\n" "gs-$w" "-" "${C_RED}stopped${C_RESET}" "-"
		fi
	done
	local missing; missing="$(gs_data_missing || true)"
	[ -n "$missing" ] && echo && echo "${C_YELLOW}note: game data incomplete ($missing) - gs cannot start until it is placed in gs/data/${C_RESET}"
	return 0
}

# --------------------------------------------------------------- console ----
cmd_console() {
	local name="${2:-}" d n
	[ -z "$name" ] && { fail "usage: ./pwctl.sh console <name>"; return 1; }
	for d in "${DAEMONS[@]}"; do
		n="$(daemon_field "$d" 1)"
		if [ "$n" = "$name" ]; then
			[ "$n" != logservice ] && is_running "$n" && { fail "$name is already running - stop it first"; return 1; }
			info "running in foreground from $(daemon_field "$d" 2); Ctrl-C to abort"
			cd "$(daemon_field "$d" 2)" && exec $(exe_fix "$(daemon_field "$d" 3)")
		fi
	done
	[ "${name#gs-}" != "$name" ] && { info "running gs world ${name#gs-} in foreground; Ctrl-C to abort"; cd gs && exec $(exe_fix "./gs") "${name#gs-}"; }
	fail "unknown daemon: $name"
	return 1
}

# ------------------------------------------------------------------ main ----
case "${1:-start}" in
	start)   cmd_start   "$@" ;;
	stop)    cmd_stop    "$@" ;;
	restart) shift; cmd_stop "$@"; echo; cmd_start "$@" ;;
	status)  cmd_status ;;
	logs)    tail_log "${2:-gdeliveryd}" -f ;;
	logtail) tail_log "${2:-gdeliveryd}" ;;
	console) cmd_console "$@" ;;
	-h|--help|help) usage ;;
	*) fail "unknown command: $1"; usage ;;
esac
