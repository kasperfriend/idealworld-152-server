/* Win32 implementation of the zig-only pthread shim (win32/zigonly/pthread.h).
 *
 * Only what the game tree uses: threads (join/detach/self/equal/exit),
 * mutex(+attr), spinlock, rwlock, cond(+timedwait), TLS keys with destructor
 * support, once, and a sigmask stub.  Linked into every daemon through the
 * driver's ld wrapper (zig mode only; native builds use real winpthreads).
 *
 * Thread model notes:
 *  - pthread_t is {tid, ev}: `ev` is a per-thread auto-reset event that the
 *    exit trampoline signals right before _endthreadex (whose own handle is
 *    auto-closed, hence unusable for join).  The trampoline owns a duplicate
 *    of `ev`, so detach (which closes the original) can never invalidate
 *    the handle the exiting thread signals.
 *  - A small tid->event table lets pthread_detach(pthread_self()) close the
 *    creator-side event even though the creator discarded its pthread_t copy
 *    (cgame threadpool does exactly that); without it every task would leak
 *    a kernel handle.
 *  - TLS key destructors run in the exiting thread via the same trampoline
 *    (POSIX runs them at thread exit; plain TlsAlloc has no such hook).
 */
#include "pthread.h"

#include <process.h>
#include <stdlib.h>
#include <string.h>

namespace {

/* --- tiny tid -> event table (also carries exit retval) ---------------- */
struct ThreadSlot {
    DWORD tid;
    HANDLE ev;
    void *retval;
};
enum { WP_MAX_THREADS = 2048 };
ThreadSlot g_slots[WP_MAX_THREADS];
LONG g_nslots = 0;
SRWLOCK g_slock = SRWLOCK_INIT;

ThreadSlot *wp_find(DWORD tid)
{
    for (LONG i = 0; i < g_nslots; i++)
        if (g_slots[i].tid == tid)
            return &g_slots[i];
    return 0;
}

int wp_insert(DWORD tid, HANDLE ev)
{
    int rc = 0;
    AcquireSRWLockExclusive(&g_slock);
    ThreadSlot *old = wp_find(tid);
    if (old) {
        /* Tid reused while a previous occupant was never reaped: drop the
         * stale event so it can never be misattributed. */
        CloseHandle(old->ev);
        old->ev = ev;
        old->retval = 0;
    } else if (g_nslots < WP_MAX_THREADS) {
        g_slots[g_nslots].tid = tid;
        g_slots[g_nslots].ev = ev;
        g_slots[g_nslots].retval = 0;
        g_nslots++;
    } else {
        rc = EAGAIN;
    }
    ReleaseSRWLockExclusive(&g_slock);
    return rc;
}

/* Removes tid; returns its event (or NULL).  Caller closes it. */
HANDLE wp_remove(DWORD tid, void **retval)
{
    HANDLE ev = NULL;
    AcquireSRWLockExclusive(&g_slock);
    for (LONG i = 0; i < g_nslots; i++) {
        if (g_slots[i].tid == tid) {
            ev = g_slots[i].ev;
            if (retval)
                *retval = g_slots[i].retval;
            g_slots[i] = g_slots[g_nslots - 1];
            g_nslots--;
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_slock);
    return ev;
}

void wp_store_retval(DWORD tid, void *retval)
{
    AcquireSRWLockExclusive(&g_slock);
    ThreadSlot *s = wp_find(tid);
    if (s)
        s->retval = retval;
    ReleaseSRWLockExclusive(&g_slock);
}

/* --- TLS key destructor registry -------------------------------------- */
struct KeyReg {
    pthread_key_t key;
    void (*dtor)(void *);
};
enum { WP_MAX_KEYS = 64 };
KeyReg g_keys[WP_MAX_KEYS];
LONG g_nkeys = 0;

void wp_run_key_dtors(void)
{
    /* POSIX iterates up to PTHREAD_DESTRUCTOR_ITERATIONS (4). */
    for (int pass = 0; pass < 4; pass++) {
        int ran = 0;
        AcquireSRWLockShared(&g_slock);
        for (LONG i = 0; i < g_nkeys; i++) {
            void *v = TlsGetValue(g_keys[i].key);
            if (v && g_keys[i].dtor) {
                TlsSetValue(g_keys[i].key, NULL);
                /* Dtor runs without the lock (it may call back in). */
                ReleaseSRWLockShared(&g_slock);
                g_keys[i].dtor(v);
                ran = 1;
                AcquireSRWLockShared(&g_slock);
            }
        }
        ReleaseSRWLockShared(&g_slock);
        if (!ran)
            break;
    }
}

/* --- thread trampoline ------------------------------------------------- */
struct StartInfo {
    void *(*fn)(void *);
    void *arg;
    HANDLE ev; /* private duplicate, signalled at exit */
};

unsigned __stdcall wp_trampoline(void *p)
{
    StartInfo *si = (StartInfo *)p;
    void *(*fn)(void *) = si->fn;
    void *arg = si->arg;
    HANDLE ev = si->ev;
    free(si);
    void *ret = fn(arg);
    wp_store_retval(GetCurrentThreadId(), ret);
    wp_run_key_dtors();
    SetEvent(ev);
    CloseHandle(ev);
    _endthreadex(0);
    return 0;
}

/* --- mutex lazy init --------------------------------------------------- */
BOOL CALLBACK wp_mutex_once(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    (void)once;
    (void)ctx;
    pthread_mutex_t *m = (pthread_mutex_t *)param;
    InitializeCriticalSection(&m->cs);
    return TRUE;
}

inline void wp_mutex_ensure(pthread_mutex_t *m)
{
    PVOID ctx = 0;
    InitOnceExecuteOnce(&m->once, wp_mutex_once, m, &ctx);
}

} /* namespace */

/* --- threads ----------------------------------------------------------- */
extern "C" {

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg)
{
    (void)attr;
    if (!thread || !start_routine)
        return EINVAL;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!ev)
        return EAGAIN;
    HANDLE evdup = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), ev, GetCurrentProcess(), &evdup,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        CloseHandle(ev);
        return EAGAIN;
    }
    StartInfo *si = (StartInfo *)malloc(sizeof(StartInfo));
    if (!si) {
        CloseHandle(evdup);
        CloseHandle(ev);
        return EAGAIN;
    }
    si->fn = start_routine;
    si->arg = arg;
    si->ev = evdup;
    unsigned tid = 0;
    uintptr_t h = _beginthreadex(NULL, 0, wp_trampoline, si, 0, &tid);
    if (h == 0) {
        free(si);
        CloseHandle(evdup);
        CloseHandle(ev);
        return EAGAIN;
    }
    CloseHandle((HANDLE)h); /* auto-closed at _endthreadex; not needed */
    if (wp_insert((DWORD)tid, ev) != 0) {
        /* Table full: thread still runs; join/detach by tid still work
         * through the returned event, only self-detach lookup is lost. */
    }
    thread->tid = (DWORD)tid;
    thread->ev = ev;
    return 0;
}

int pthread_join(pthread_t thread, void **retval)
{
    if (thread.tid == GetCurrentThreadId())
        return EDEADLK;
    HANDLE ev = thread.ev;
    if (!ev) {
        /* Only a tid (e.g. pthread_self of another thread): look it up. */
        AcquireSRWLockShared(&g_slock);
        ThreadSlot *s = wp_find(thread.tid);
        ev = s ? s->ev : NULL;
        ReleaseSRWLockShared(&g_slock);
        if (!ev)
            return ESRCH;
    }
    if (WaitForSingleObject(ev, INFINITE) != WAIT_OBJECT_0)
        return ESRCH;
    void *ret = 0;
    HANDLE owned = wp_remove(thread.tid, &ret);
    if (owned)
        CloseHandle(owned);
    else if (ev == thread.ev)
        CloseHandle(ev);
    if (retval)
        *retval = ret;
    return 0;
}

int pthread_detach(pthread_t thread)
{
    void *ret = 0;
    HANDLE owned = wp_remove(thread.tid, &ret);
    if (owned) {
        CloseHandle(owned);
        return 0;
    }
    if (thread.ev) {
        CloseHandle(thread.ev);
        return 0;
    }
    return 0;
}

pthread_t pthread_self(void)
{
    pthread_t t;
    t.tid = GetCurrentThreadId();
    t.ev = NULL;
    return t;
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    return t1.tid == t2.tid;
}

void pthread_exit(void *retval)
{
    wp_store_retval(GetCurrentThreadId(), retval);
    wp_run_key_dtors();
    HANDLE ev = NULL;
    AcquireSRWLockShared(&g_slock);
    ThreadSlot *s = wp_find(GetCurrentThreadId());
    if (s)
        ev = s->ev;
    ReleaseSRWLockShared(&g_slock);
    if (ev)
        SetEvent(ev);
    _endthreadex(0);
}

/* --- mutex -------------------------------------------------------------- */
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *attr)
{
    if (!m)
        return EINVAL;
    InitOnceInitialize(&m->once);
    m->locks = 0;
    m->kind = attr ? attr->kind : PTHREAD_MUTEX_DEFAULT;
    /* cs initialized lazily so PTHREAD_MUTEX_INITIALIZER statics work too */
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *m)
{
    if (!m)
        return EINVAL;
    if (m->locks != 0)
        return EBUSY;
    wp_mutex_ensure(m);
    DeleteCriticalSection(&m->cs);
    InitOnceInitialize(&m->once); /* allow(rare) reuse after destroy */
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *m)
{
    if (!m)
        return EINVAL;
    wp_mutex_ensure(m);
    EnterCriticalSection(&m->cs);
    InterlockedIncrement(&m->locks);
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *m)
{
    if (!m)
        return EINVAL;
    wp_mutex_ensure(m);
    if (!TryEnterCriticalSection(&m->cs))
        return EBUSY;
    InterlockedIncrement(&m->locks);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m)
{
    if (!m)
        return EINVAL;
    InterlockedDecrement(&m->locks);
    LeaveCriticalSection(&m->cs);
    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    if (!attr)
        return EINVAL;
    attr->kind = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    (void)attr;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind)
{
    if (!attr)
        return EINVAL;
    attr->kind = kind;
    return 0;
}

/* --- spinlock ------------------------------------------------------------ */
int pthread_spin_init(pthread_spinlock_t *s, int pshared)
{
    (void)pshared;
    if (!s)
        return EINVAL;
    s->v = 0;
    return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *s)
{
    (void)s;
    return 0;
}

int pthread_spin_lock(pthread_spinlock_t *s)
{
    if (!s)
        return EINVAL;
    while (InterlockedCompareExchange(&s->v, 1, 0) != 0)
        SwitchToThread();
    return 0;
}

int pthread_spin_trylock(pthread_spinlock_t *s)
{
    if (!s)
        return EINVAL;
    return InterlockedCompareExchange(&s->v, 1, 0) == 0 ? 0 : EBUSY;
}

int pthread_spin_unlock(pthread_spinlock_t *s)
{
    if (!s)
        return EINVAL;
    InterlockedExchange(&s->v, 0);
    return 0;
}

/* --- rwlock ----------------------------------------------------------------
 * `ex` records whether the current holder is a writer.  This is exact: a
 * writer holds the SRW exclusively (no reader can be inside to observe a
 * stale 0), and readers can only be inside while no writer holds it. */
int pthread_rwlock_init(pthread_rwlock_t *rw, const pthread_rwlockattr_t *attr)
{
    (void)attr;
    if (!rw)
        return EINVAL;
    InitializeSRWLock(&rw->l);
    rw->ex = 0;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rw)
{
    (void)rw;
    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rw)
{
    if (!rw)
        return EINVAL;
    AcquireSRWLockShared(&rw->l);
    return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *rw)
{
    if (!rw)
        return EINVAL;
    return TryAcquireSRWLockShared(&rw->l) ? 0 : EBUSY;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rw)
{
    if (!rw)
        return EINVAL;
    AcquireSRWLockExclusive(&rw->l);
    InterlockedExchange(&rw->ex, 1);
    return 0;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *rw)
{
    if (!rw)
        return EINVAL;
    if (!TryAcquireSRWLockExclusive(&rw->l))
        return EBUSY;
    InterlockedExchange(&rw->ex, 1);
    return 0;
}

int pthread_rwlock_unlock(pthread_rwlock_t *rw)
{
    if (!rw)
        return EINVAL;
    if (InterlockedCompareExchange(&rw->ex, 0, 1) == 1)
        ReleaseSRWLockExclusive(&rw->l);
    else
        ReleaseSRWLockShared(&rw->l);
    return 0;
}

/* --- condition variables ---------------------------------------------------- */
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *attr)
{
    (void)attr;
    if (!c)
        return EINVAL;
    InitializeConditionVariable(&c->cv);
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *c)
{
    (void)c;
    return 0;
}

int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
    if (!c || !m)
        return EINVAL;
    wp_mutex_ensure(m);
    InterlockedDecrement(&m->locks);
    SleepConditionVariableCS(&c->cv, &m->cs, INFINITE);
    InterlockedIncrement(&m->locks);
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m,
                           const struct timespec *abstime)
{
    DWORD ms = INFINITE;
    if (!c || !m || !abstime)
        return EINVAL;
    wp_mutex_ensure(m);
    /* abstime is CLOCK_REALTIME (callers build it from gettimeofday). */
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULONGLONG now100 = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    LONGLONG abs100 = (LONGLONG)((abstime->tv_sec + 11644473600LL) * 10000000LL +
                                 abstime->tv_nsec / 100);
    LONGLONG delta_ms = (abs100 - (LONGLONG)now100) / 10000;
    if (delta_ms < 0)
        return ETIMEDOUT;
    if (delta_ms < (LONGLONG)(INFINITE - 1))
        ms = (DWORD)delta_ms;
    else
        ms = INFINITE - 1;
    InterlockedDecrement(&m->locks);
    BOOL ok = SleepConditionVariableCS(&c->cv, &m->cs, ms);
    InterlockedIncrement(&m->locks);
    if (!ok && GetLastError() == ERROR_TIMEOUT)
        return ETIMEDOUT;
    return 0;
}

int pthread_cond_signal(pthread_cond_t *c)
{
    if (!c)
        return EINVAL;
    WakeConditionVariable(&c->cv);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *c)
{
    if (!c)
        return EINVAL;
    WakeAllConditionVariable(&c->cv);
    return 0;
}

/* --- thread-local storage ---------------------------------------------------- */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
    DWORD k;
    if (!key)
        return EINVAL;
    k = TlsAlloc();
    if (k == TLS_OUT_OF_INDEXES)
        return ENOMEM;
    if (destructor) {
        AcquireSRWLockExclusive(&g_slock);
        if (g_nkeys < WP_MAX_KEYS) {
            g_keys[g_nkeys].key = k;
            g_keys[g_nkeys].dtor = destructor;
            g_nkeys++;
        }
        ReleaseSRWLockExclusive(&g_slock);
    }
    *key = k;
    return 0;
}

int pthread_key_delete(pthread_key_t key)
{
    AcquireSRWLockExclusive(&g_slock);
    for (LONG i = 0; i < g_nkeys; i++) {
        if (g_keys[i].key == key) {
            g_keys[i] = g_keys[g_nkeys - 1];
            g_nkeys--;
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_slock);
    return TlsFree(key) ? 0 : EINVAL;
}

void *pthread_getspecific(pthread_key_t key)
{
    return TlsGetValue(key);
}

int pthread_setspecific(pthread_key_t key, const void *value)
{
    return TlsSetValue(key, (LPVOID)value) ? 0 : EINVAL;
}

/* --- one-time init ------------------------------------------------------------- */
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    if (!once_control || !init_routine)
        return EINVAL;
    if (*once_control == 2)
        return 0;
    if (InterlockedCompareExchange(once_control, 1, 0) == 0) {
        init_routine();
        InterlockedExchange(once_control, 2);
    } else {
        while (*once_control != 2)
            SwitchToThread();
    }
    return 0;
}

/* --- signals -------------------------------------------------------------------- */
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
    (void)how;
    (void)set;
    if (oldset)
        memset(oldset, 0, sizeof(*oldset));
    return 0;
}

} /* extern "C" */
