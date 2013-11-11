/*
 * plasma_spin - portable macros for processor-specific spin loops
 *
 *   inline assembly and compiler intrinsics for building spin loops / spinlocks
 *
 *
 * Copyright (c) 2012, Glue Logic LLC. All rights reserved. code()gluelogic.com
 *
 *  This file is part of mcdb.
 *
 *  mcdb is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  mcdb is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with mcdb.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _XOPEN_SOURCE
#ifdef __cplusplus
#define _XOPEN_SOURCE 500
#else
#define _XOPEN_SOURCE 600
#endif
#endif

#define PLASMA_FEATURE_DISABLE_WIN32_FULLHDRS
#define PLASMA_SPIN_C99INLINE_FUNCS

/* inlined functions defined in header
 * (generate external linkage definition in GCC versions earlier than GCC 4.3)*/
#if defined(NO_C99INLINE) \
 || defined(__clang__) || (defined(__GNUC__) && !defined(__GNUC_STDC_INLINE__))
#define PLASMA_SPIN_C99INLINE
#endif

#include "plasma_spin.h"
#include "plasma_membar.h"
#include "plasma_sysconf.h"


/*
 * simple spin lock
 */

/* FUTURE: might consider using plasma_spin_pause_yield_adaptive() in place of
 * direct call to plasma_spin_pause() in some tight spin loops.  Keeping track
 * of callcount has some overhead, but on the other hand permits yielding CPU
 * if spin continues for too long.  TBD. */
/* FUTURE: might experiment with implementing plasma_atomic_lock_acquire()
 * using CAS or xchg or plasma_atomic_fetch_add(), which might be slightly more
 * performant depending on the platform architecture */


#ifndef plasma_spin_lock_acquire_spinloop
bool
plasma_spin_lock_acquire_spinloop (plasma_spin_lock_t * const spin)
{
    uint32_t * const lck = &spin->lck;
    do {
        while (plasma_atomic_load_explicit(lck, memory_order_relaxed)) {
            plasma_spin_pause();
        }
    } while (!plasma_atomic_lock_acquire(lck)); /*(includes barrier)*/
    return true;
}
#endif

bool
plasma_spin_lock_acquire_spindecay (plasma_spin_lock_t * const spin,
                                    int pause1, int pause32, int yield)
{
    /* ((uint32_t *) cast also works for Apple OSSpinLock, which is int32_t) */
    uint32_t * const lck = (uint32_t *)&spin->lck;
    do {
        while (plasma_atomic_load_explicit(lck, memory_order_relaxed)) {
            if (pause1) {
                --pause1;
                plasma_spin_pause();
            }
            else if (pause32) {
                int i = 4;
                --pause32;
                do {
                    plasma_spin_pause();
                    plasma_spin_pause();
                    plasma_spin_pause();
                    plasma_spin_pause();
                    plasma_spin_pause();
                    plasma_spin_pause();
                    plasma_spin_pause();
                    plasma_spin_pause();
                } while (--i);
            }
            else if (yield) {
                --yield;
                plasma_spin_yield();
            }
            else
                return false;
        }
    } while (!plasma_atomic_lock_acquire(lck)); /*(includes barrier)*/
    return true;
}


static uint32_t nshift; /* num bits rotate (shift) for taglock tag batch */
static uint32_t nprocs; /* num procs */

__attribute_cold__
__attribute_noinline__
static int
plasma_spin_nprocs_init (void)
{
    long nprocs_onln = plasma_sysconf_nprocessors_onln();
    uint32_t nshift_onln;
    uint32_t nshift_accum = 0;

    if (nprocs_onln < 1)
        nprocs_onln = 1;

    nshift_onln = (uint32_t)nprocs_onln;
    while ((nshift_onln >>= 1))
        ++nshift_accum;
    nshift = nshift_accum;  /* power of 2 of available CPUs (0 if only 1 CPU) */
    plasma_membar_StoreStore();
    return (nprocs = (uint32_t)nprocs_onln);

    /* store ordering above is not critical; so our readers below do not acquire
     * If there is a race between threads before nprocs is set, then if a thread
     * reads nshift = 0, but nprocs != 0, then thread might be slightly slower
     * acquiring the spin lock the first time.  Not a big deal.  Likewise, it
     * is not problem if multiple threads run this routine since this routine
     * stores results atomically (via int-sized natural alignment) to the
     * static variables and the results of initialization are the same. */
}


C99INLINE
static void
plasma_spin_pause_yield_adaptive (const uint32_t distance, const int callcount)
{
    /* simplistic backoff employing call count and distance to acquire fair lock
     * (might be adjusted to pause multiple times for slightly larger input
     *  distances, especially on machines with larger numbers of CPUs/cores/SMT,
     *  but be sure to always yield quickly/immediately if distance >= nprocs)
     *
     * callers must initialize nshift and nprocs prior to calling this routine
     * (initialization avoided here since code might be inlined in tight loop)
     * (ok if a few threads run the initialization; results will be same)
     *    if (__builtin_expect( (!nprocs), 0))
     *        plasma_spin_nprocs_init();
     *
     * (nshift == 0 for one core; always yield CPU
     *  nshift == 1 for two cores; test distance <= nshift)
     */
    if (callcount < 128 && distance <= nshift) {
        plasma_spin_pause();  /* brief pause if almost our turn */
    }
    else {
        plasma_spin_yield();  /* yield CPU if highly contended */
    }
}


/*
 * ticket lock
 *
 * A.3 Ticket Lock provides description and sample ticket lock impl for x86:
 * http://www.intel.com/content/dam/www/public/us/en/documents/white-papers/xeon-lock-scaling-analysis-paper.pdf
 * (but in sample implementation in paper above, the unlock should have release
 * fence prior to the increment, or should perform the increment in a temporary
 * variable and then use an atomic store operation with memory_order_release)
 */

bool
plasma_spin_tktlock_acquire_spinloop (plasma_spin_tktlock_t *
                                        const restrict spin, uint32_t tkt)
{
    /* spin until ticket num ready to be served matches our position in queue */
    uint32_t cmp = PLASMA_SPIN_TKTLOCK_MASK(tkt);
    const uint32_t tktnum = PLASMA_SPIN_TKTLOCK_SHIFT(tkt);
    int callcount = 0;
    if (__builtin_expect( (!nprocs), 0))
        plasma_spin_nprocs_init(); /*init plasma_spin_pause_yield_adaptive()*/
    do {
        cmp = tktnum > cmp  /*(reuse cmp to store distance)*/
          ? tktnum - cmp
          : (PLASMA_SPIN_TKTLOCK_TKTMAX + 1 - cmp) + tktnum;
        plasma_spin_pause_yield_adaptive(cmp, ++callcount);
      #ifndef __ia64__
        cmp = PLASMA_SPIN_TKTLOCK_MASK(
                plasma_atomic_load_explicit(&spin->lck.u,memory_order_relaxed));
      #else  /*(Itanium should emit ld4.acq in atomic load below)*/
        cmp = PLASMA_SPIN_TKTLOCK_MASK(
                plasma_atomic_load_explicit(&spin->lck.u,memory_order_acquire));
      #endif
    } while (tktnum != cmp);
  #ifndef __ia64__  /*(Itanium should emit ld4.acq in atomic load above)*/
    atomic_thread_fence(memory_order_acquire);
  #endif
    return true;
}


/* inlined functions defined in header
 * (generate external linkage definition in C99-compliant compilers)
 * (need to -duplicate- definition from header for non-C99-compliant compilers)
 */
#if !defined(__GNUC__) || defined(__GNUC_STDC_INLINE__)
extern inline
bool
plasma_spin_tktlock_is_free (const plasma_spin_tktlock_t * const restrict spin);
bool
plasma_spin_tktlock_is_free (const plasma_spin_tktlock_t * const restrict spin);

extern inline
bool
plasma_spin_tktlock_acquire (plasma_spin_tktlock_t * const restrict spin);
bool
plasma_spin_tktlock_acquire (plasma_spin_tktlock_t * const restrict spin);

extern inline
void
plasma_spin_tktlock_release (plasma_spin_tktlock_t * const restrict spin);
void
plasma_spin_tktlock_release (plasma_spin_tktlock_t * const restrict spin);
#endif


/*
 * tag lock
 */

/* FUTURE: might experiment with having both tag and lck as 16-bit quantities
 * in a single 32-bit word, and using CAS to modify both in a single atomic op.
 * Using CAS does require a read prior to the atomic write, which has its own
 * costs.  Why bother?  Currently plasma_spin_taglock_acquire() is implemented
 * with two atomics, which in the uncontended case is slower than tktlock, which
 * uses a single atomic operation.  (Might also look into cmpxchg8 for 64-bit
 * CAS of both tag and lck.)  Storing tag and lck in 16-bit would limit number
 * of potential simultaneous lock contenders to 32767 (SHRT_MAX) plus one with
 * the lock.  (Traditional ticket lock has limit of 65535 (USHRT_MAX) plus one
 * with the lock.) */

#define PLASMA_SPIN_TAGLOCK_BATCH(x)  ((x)>>nshift)

bool
plasma_spin_taglock_acquire (plasma_spin_taglock_t * const restrict taglock)
{
    if (__builtin_expect( (!nprocs), 0))/*init for PLASMA_SPIN_TAGLOCK_BATCH()*/
        plasma_spin_nprocs_init(); /*init plasma_spin_pause_yield_adaptive()*/

    uint32_t * const restrict lck = (uint32_t *)&taglock->lck;
    const uint32_t tag = PLASMA_SPIN_TAGLOCK_MASK(
                           plasma_atomic_fetch_add_u32(&taglock->tag, 1,
                                                       memory_order_relaxed));
    uint32_t cmp = PLASMA_SPIN_TAGLOCK_MASK(*lck);
    const uint32_t batch = PLASMA_SPIN_TAGLOCK_BATCH(tag);
    int callcount = 0;

    /* pause/yield until tag is part of the current batch */
    while (PLASMA_SPIN_TAGLOCK_BATCH(cmp) != batch) {
        cmp = tag > cmp  /*(reuse cmp to store distance)*/
          ? tag - cmp
          : (PLASMA_SPIN_TAGLOCK_TAGMAX + 1 - cmp) + tag;
        plasma_spin_pause_yield_adaptive(cmp, ++callcount);
        cmp = PLASMA_SPIN_TAGLOCK_MASK(
                plasma_atomic_load_explicit(lck, memory_order_relaxed));
    }

    /* attempt to obtain lock, or else spin and retry
     * (above PLASMA_SPIN_TAGLOCK_BATCH(cmp) == batch only when lock is free) */
  #ifdef PLASMA_SPIN_TAGLOCK_VIA_FETCH_OR
    while (PLASMA_SPIN_TAGLOCK_IS_LOCKED(
             plasma_atomic_fetch_or_u32(lck, PLASMA_SPIN_TAGLOCK_ORVAL,
                                        memory_order_relaxed))) {
        do { plasma_spin_pause();
        } while (PLASMA_SPIN_TAGLOCK_IS_LOCKED(
                   plasma_atomic_load_explicit(lck, memory_order_relaxed)));
    }
  #else  /* (needs the cmp assignment below for reuse in the CAS) */
    while (!plasma_atomic_CAS_32(lck,cmp,PLASMA_SPIN_TAGLOCK_LOCKVAL(cmp))) {
        do {
            plasma_spin_pause();
            cmp = plasma_atomic_load_explicit(lck, memory_order_relaxed);
        } while (PLASMA_SPIN_TAGLOCK_IS_LOCKED(cmp));
    }
  #endif
    plasma_membar_atomic_thread_fence_acq_rel();
    return true;
}

bool
plasma_spin_taglock_acquire_urgent (plasma_spin_taglock_t *
                                      const restrict taglock)
{
  #ifdef PLASMA_SPIN_TAGLOCK_VIA_FETCH_OR
    uint32_t * const restrict lck = (uint32_t *)&taglock->lck;
    uint32_t cmp;
    while (PLASMA_SPIN_TAGLOCK_IS_LOCKED(
             (cmp = plasma_atomic_fetch_or_u32(lck, PLASMA_SPIN_TAGLOCK_ORVAL,
                                               memory_order_relaxed)))) {
        do { plasma_spin_pause();
        } while (PLASMA_SPIN_TAGLOCK_IS_LOCKED(
                   plasma_atomic_load_explicit(lck, memory_order_relaxed)));
    }
    /* jumped queue by locking without incrementing tag
     * (therefore decrement lck since taglock_release will increment lck) */
    *lck = PLASMA_SPIN_TAGLOCK_LOCKVAL(cmp-1);
    plasma_membar_atomic_thread_fence_acq_rel();
    return true;
  #else
    uint32_t * const restrict lck = (uint32_t *)&taglock->lck;
    uint32_t cmp = *lck;
    do {
        while (PLASMA_SPIN_TAGLOCK_IS_LOCKED(cmp)) {
            plasma_spin_pause();
            cmp = plasma_atomic_load_explicit(lck, memory_order_relaxed);
        }                                               /*(jump queue:(cmp-1))*/
    } while (!plasma_atomic_CAS_32(lck,cmp,PLASMA_SPIN_TAGLOCK_LOCKVAL(cmp-1)));
    plasma_membar_atomic_thread_fence_acq_rel();
    return true;
  #endif
}


/* inlined functions defined in header
 * (generate external linkage definition in C99-compliant compilers)
 * (need to -duplicate- definition from header for non-C99-compliant compilers)
 */
#if !defined(__GNUC__) || defined(__GNUC_STDC_INLINE__)
extern inline
bool
plasma_spin_taglock_acquire_try (plasma_spin_taglock_t *
                                   const restrict taglock);
bool
plasma_spin_taglock_acquire_try (plasma_spin_taglock_t *
                                   const restrict taglock);

extern inline
void
plasma_spin_taglock_release (plasma_spin_taglock_t * const restrict taglock);
void
plasma_spin_taglock_release (plasma_spin_taglock_t * const restrict taglock);
#endif
