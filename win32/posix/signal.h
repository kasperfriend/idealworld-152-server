/* Windows shim: <signal.h> - extends the MSVCRT one with the POSIX surface
 * the server uses (extra signal numbers, sigset_t, sigaction, ...). */
#ifndef _WP_SIGNAL_H
#define _WP_SIGNAL_H
#include_next <signal.h>

#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGUSR1
#define SIGUSR1 10
#endif
#ifndef SIGUSR2
#define SIGUSR2 12
#endif
#ifndef SIGPIPE
#define SIGPIPE 13
#endif
#ifndef SIGALRM
#define SIGALRM 14
#endif
#ifndef SIGCHLD
#define SIGCHLD 17
#endif
#ifndef SIGBUS
#define SIGBUS 7
#endif
#ifndef SIGSTOP
#define SIGSTOP 23
#endif
#ifndef SIGCONT
#define SIGCONT 25
#endif
#ifndef SIGURG
#define SIGURG 23
#endif
#ifndef SIGSYS
#define SIGSYS 31
#endif

#ifndef SA_RESTART
#define SA_RESTART 0
#endif
#ifndef SA_NOCLDSTOP
#define SA_NOCLDSTOP 0
#endif
#ifndef SA_SIGINFO
#define SA_SIGINFO 0
#endif

#ifndef SIG_BLOCK
#define SIG_BLOCK 0
#endif
#ifndef SIG_UNBLOCK
#define SIG_UNBLOCK 1
#endif
#ifndef SIG_SETMASK
#define SIG_SETMASK 2
#endif

#ifndef _WP_SIGSET_T_DEFINED
#define _WP_SIGSET_T_DEFINED
typedef struct { unsigned long __bits[8]; } sigset_t;
#endif

#ifndef _WP_SIGACTION_DEFINED
#define _WP_SIGACTION_DEFINED
struct sigaction
{
	void (*sa_handler)(int);
	sigset_t sa_mask;
	int sa_flags;
	void (*sa_sigaction)(int, void *, void *);
};
#endif

#ifdef __cplusplus
extern "C" {
#endif
#ifndef _WP_SIGFUNCS_DECLARED
#define _WP_SIGFUNCS_DECLARED
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int sig);
int sigdelset(sigset_t *set, int sig);
int sigismember(const sigset_t *set, int sig);
int sigprocmask(int how, const sigset_t *set, sigset_t *old);
int sigaction(int sig, const struct sigaction *act,
              struct sigaction *oldact);
int kill(int pid, int sig);
#endif
#ifdef __cplusplus
}
#endif
#endif
