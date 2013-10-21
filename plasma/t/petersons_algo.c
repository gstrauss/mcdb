/* Peterson's algorithm for mutual exclusion between two threads
 * http://en.wikipedia.org/wiki/Peterson%27s_algorithm
 *
 * C implementation of Dmitriy V'jukov code referenced at
 * http://www.justsoftwaresolutions.co.uk/threading/petersons_lock_with_C++0x_atomics.html
 *
 * lock acquisition:
 * - flag interest in obtaining lock
 * - atomically give other thread preference to obtain lock
 *   (synchornization point)
 * - wait for other thread to release lock, if other had flagged interest
 *   (otherwise, other thread will run same code, setting turn to *this* thread)
 *
 * lock release:
 * - clear flag indicating interest in obtaining lock
 *
 * atomic operations:
 * - atomic exchange setting 'turn' is thread synchronization point
 *   (memory_order_acq_rel)
 * - memory_order_acquire of flag[other] in lock acquisition parallels
 *   memory_order_release in lock release code
 * - memory_order_relaxed used for remaining atomic loads and stores
 *   - store flagging interest in obtaining lock is ordered by
 *     subsequent atomic exchange on 'turn'
 *   - load of 'turn' in loop is ordered by acquire of flag[other]
 */

#ifndef _XOPEN_SOURCE
#ifdef __cplusplus
#ifdef _AIX
#define __STDC_FORMAT_MACROS
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif
#if defined(__sun) && defined(__SVR4) /* define __EXTENSIONS__ for PRIu64 */
#define __EXTENSIONS__
#endif
#else
#define _XOPEN_SOURCE 600
#endif
#endif

#include "../plasma_attr.h"
#include "../plasma_atomic.h"
#include "../plasma_spin.h"
#include "../plasma_stdtypes.h"

struct petersons_algo_t {
  uint32_t flag[2];  /* each thread indicates interest in obtaining lock */
  uint32_t turn;     /* indicate whose turn it is to obtain lock */
};

static void
petersons_lock_acquire (struct petersons_algo_t * const restrict p,
                        const uint32_t thrnum)
{
    const uint32_t other = 1 - thrnum;  /* index of other thread */
    plasma_atomic_store_explicit(&p->flag[thrnum], 1u, memory_order_relaxed);
    plasma_atomic_exchange_n_32(&p->turn, other, memory_order_acq_rel);
    while (plasma_atomic_load_explicit(&p->flag[other], memory_order_acquire)
        && plasma_atomic_load_explicit(&p->turn, memory_order_relaxed)==other)
        plasma_spin_pause();
}

static void
petersons_lock_release (struct petersons_algo_t * const restrict p,
                        const uint32_t thrnum)
{
    plasma_atomic_store_explicit(&p->flag[thrnum], 0u, memory_order_release);
}


/*
 * Exercise Peterson's algorithm for mutual exclusion between two threads
 *
 * recent gcc/clang
 *   $ gcc -std=c99 -O3 petersons_algo.c -lpthread
 * other compilers
 *   $ cc petersons_algo.c ../plasma_atomic.c -lpthread
 */


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

static pthread_barrier_t sync_start_barrier;
static struct petersons_algo_t plock;         /* (zero-initialized) */
static int circuit;                           /* num of lock test iterations */
static uint64_t tc = 0;                       /* total count of locking */
static int rc;                                /* return code; 0 for no errs */


__attribute_noinline__
static bool
check_threads_done (const int thrnum)
{
    static int thread_status[2];  /* Peterson's algorithm between two threads */
    const uint32_t other = 1 - thrnum;  /* other thread num */
    /*(no need for plasma_atomic_store_explicit() for stores here
     * since values in thread_status[] are changed only to true) */
    if (!thread_status[thrnum])
        thread_status[thrnum] = true;
    return plasma_atomic_load_explicit(thread_status+other,
                                       memory_order_consume);
}

#ifdef __cplusplus
extern "C" {
#endif
static void *
spinner (void * const arg)
{
    const int thrnum = (int)(uintptr_t)arg;
    const int max = circuit;
    int i = -1;
    int errs = 0;
    uint64_t dist_b;
    uint64_t dist_e = 0;

    (void)pthread_barrier_wait(&sync_start_barrier);

    while (++i < max || !check_threads_done(thrnum)) {

        dist_b = plasma_atomic_load_explicit(&tc, memory_order_acquire);
        petersons_lock_acquire(&plock, thrnum);
        dist_e = ++tc;  /*atomic not needed; modify tc within critical section*/
        petersons_lock_release(&plock, thrnum);

        if (__builtin_expect( (dist_e <= dist_b), 0))
            errs++;     /*(corruption; should not happen)*/
    }

    fprintf(stderr, "(thr %d) tc:%"PRIu64" num:%d\n", thrnum, dist_e, (i-errs));
    if (errs) {
        plasma_atomic_store(&rc, 1);
        fprintf(stderr, "\nerrors: %d (thr %d)\n\n", errs, thrnum);
    }

    return NULL;
}
#ifdef __cplusplus
}
#endif

int
main (int argc, char *argv[])
{
    circuit = argc > 1 ? atoi(argv[1]) : 1000000;

    pthread_t thr[2];  /* Peterson's algorithm between two threads */
    pthread_barrier_init(&sync_start_barrier, NULL, 2);
    pthread_create(&thr[0], NULL, spinner, (void *)(uintptr_t)0);
    pthread_create(&thr[1], NULL, spinner, (void *)(uintptr_t)1);
    pthread_join(thr[0], NULL);
    pthread_join(thr[1], NULL);
    pthread_barrier_destroy(&sync_start_barrier);

    fprintf(stderr, "done tc: %"PRIu64"\n",
            plasma_atomic_load_explicit(&tc, memory_order_acquire));

    return plasma_atomic_load(&rc);
}
