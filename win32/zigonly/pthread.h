/* zig-only <pthread.h> over native Win32 threads.
 *
 * zig cc ships no pthread support for x86_64-windows-gnu, so everything the
 * game uses from pthreads is declared here and implemented in
 * win32/wpthread.cpp (linked into every daemon through the driver's ld
 * wrapper).  Native MSYS2 builds never see this file: they use the real
 * winpthreads via -I ordering (this directory is only on the path in zig
 * mode).
 *
 * Implemented surface (exactly what the tree uses, plus trivially cheap
 * insurance): threads, mutex(+attr), spinlock, rwlock, cond, TLS keys
 * (with destructor support), once, sigmask stub.
 */
#ifndef WP_ZIG_PTHREAD_H
#define WP_ZIG_PTHREAD_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif

#include <windows.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- threads -------------------------------------------------------- */
typedef struct { HANDLE h; DWORD tid; } pthread_t;
typedef int pthread_attr_t; /* opaque; call sites always pass NULL */

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **retval);
int pthread_detach(pthread_t thread);
pthread_t pthread_self(void);
int pthread_equal(pthread_t t1, pthread_t t2);
void pthread_exit(void *retval);

/* --- mutex ---------------------------------------------------------- */
typedef struct {
    INIT_ONCE once;      /* lazy CRITICAL_SECTION init (statics safe) */
    LONG locks;          /* recursion depth, for EBUSY in destroy */
    int kind;
    CRITICAL_SECTION cs;
} pthread_mutex_t;
typedef struct { int kind; } pthread_mutexattr_t;

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL
#define PTHREAD_PROCESS_PRIVATE  0
#define PTHREAD_PROCESS_SHARED   1
#define PTHREAD_MUTEX_INITIALIZER {0,0,0}

int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *m);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);
int pthread_mutex_unlock(pthread_mutex_t *m);
int pthread_mutexattr_init(pthread_mutexattr_t *attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind);

/* --- spinlock ------------------------------------------------------- */
typedef struct { LONG v; } pthread_spinlock_t;

int pthread_spin_init(pthread_spinlock_t *s, int pshared);
int pthread_spin_destroy(pthread_spinlock_t *s);
int pthread_spin_lock(pthread_spinlock_t *s);
int pthread_spin_trylock(pthread_spinlock_t *s);
int pthread_spin_unlock(pthread_spinlock_t *s);

/* --- rwlock --------------------------------------------------------- */
typedef struct { SRWLOCK l; volatile LONG ex; } pthread_rwlock_t;
typedef int pthread_rwlockattr_t;

int pthread_rwlock_init(pthread_rwlock_t *rw, const pthread_rwlockattr_t *attr);
int pthread_rwlock_destroy(pthread_rwlock_t *rw);
int pthread_rwlock_rdlock(pthread_rwlock_t *rw);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rw);
int pthread_rwlock_wrlock(pthread_rwlock_t *rw);
int pthread_rwlock_trywrlock(pthread_rwlock_t *rw);
int pthread_rwlock_unlock(pthread_rwlock_t *rw);

/* --- condition variables -------------------------------------------- */
typedef struct { CONDITION_VARIABLE cv; } pthread_cond_t;
typedef int pthread_condattr_t;

int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m,
                           const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_broadcast(pthread_cond_t *c);

/* --- thread-local storage ------------------------------------------- */
typedef DWORD pthread_key_t;

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_key_delete(pthread_key_t key);
void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *value);

/* --- one-time init -------------------------------------------------- */
typedef LONG pthread_once_t;
#define PTHREAD_ONCE_INIT 0L

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

/* --- signals -------------------------------------------------------- */
/* All real pthread_sigmask call sites are already #ifndef WIN32 guarded;
 * the stub exists so any missed one still links. */
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset);

#ifdef __cplusplus
}
#endif

#endif /* WP_ZIG_PTHREAD_H */
