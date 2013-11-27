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

#ifndef INCLUDED_PLASMA_SPIN_H
#define INCLUDED_PLASMA_SPIN_H

#include "plasma_feature.h"
#include "plasma_attr.h"
#include "plasma_membar.h"
#include "plasma_atomic.h"
#include "plasma_endian.h"
PLASMA_ATTR_Pragma_once

#if defined(__APPLE__) && defined(__MACH__)
#include <libkern/OSAtomic.h>
#endif

#ifndef PLASMA_SPIN_C99INLINE
#define PLASMA_SPIN_C99INLINE C99INLINE
#endif
#ifndef NO_C99INLINE
#ifndef PLASMA_SPIN_C99INLINE_FUNCS
#define PLASMA_SPIN_C99INLINE_FUNCS
#endif
#endif

/* spinlocks / busy-wait loops that pause briefly (versus spinning on nop
 * instructions) benefit modern processors by reducing memory hazards upon loop
 * exit as well as potentially saving CPU power and reducing resource contention
 * with other CPU threads (SMT) (a.k.a strands) on the same core. */

/* plasma_spin_pause() */

/* The asm memory constraint (:::"memory") is not required for pause, but
 * if provided acts as a compiler fence to disable compiler optimization,
 * e.g. in a spin loop where the spin variable is (incorrectly) not a load
 * of _Atomic, or is not volatile.  The __volatile__ on the asm (where present
 * in the 'pause' implementations below) effects appropriate compiler fence.
 * NB: volatile -does not- appear to result in a compiler fence for IBM xlC */

#if defined(_MSC_VER)

  /* prefer using intrinsics directly instead of winnt.h macro */
  /* http://software.intel.com/en-us/forums/topic/296168 */
  #include <intrin.h>
  #if defined(_M_AMD64) || defined(_M_IX86)
  #pragma intrinsic(_mm_pause)
  #define plasma_spin_pause()  _mm_pause()
  /* (if pause not supported by older x86 assembler, "rep nop" is equivalent)*/
  /*#define plasma_spin_pause()  __asm rep nop */
  #elif defined(_M_IA64)
  #pragma intrinsic(__yield)
  #define plasma_spin_pause()  __yield()
  #else
  #define plasma_spin_pause()  YieldProcessor()
  #endif
  #if 0  /* do not set these in header; calling file should determine need */
  /* http://msdn.microsoft.com/en-us/library/windows/desktop/ms687419%28v=vs.85%29.aspx */
  /* NB: caller needs to include proper header for YieldProcessor()
   *     (e.g. #include <windows.h>)
   * http://stackoverflow.com/questions/257134/weird-compile-error-dealing-with-winnt-h */
  /* http://support.microsoft.com/kb/166474 */
  #define VC_EXTRALEAN
  #define WIN32_LEAN_AND_MEAN
  #include <wtypes.h>
  #include <windef.h>
  #include <winnt.h>
  #endif

#elif defined(__x86_64__) || defined(__i386__)

  /* http://software.intel.com/sites/products/documentation/studio/composer/en-us/2011Update/compiler_c/intref_cls/common/intref_sse2_pause.htm
   * http://software.intel.com/sites/default/files/m/2/c/d/3/9/25602-17689_w_spinlock.pdf
   * http://software.intel.com/en-us/forums/topic/309231
   * http://siyobik.info.gf/main/reference/instruction/PAUSE
   * http://stackoverflow.com/questions/7086220/what-does-rep-nop-mean-in-x86-assembly
   * http://stackoverflow.com/questions/7371869/minimum-time-a-thread-can-pause-in-linux
   */
   /* http://gcc.gnu.org/onlinedocs/gcc/X86-Built_002din-Functions.html
    * GCC provides __builtin_ia32_pause()
    *   Generates the pause machine instruction with a compiler memory barrier. 
    * (not sure if preferred vis-a-vis _mm_pause() or asm "pause" (below)) */
   /* avoid pulling in the large header xmmintrin.h just for plasma_spin_pause()
    * clang xmmintrin.h defines _mm_pause() as pause
    * #if defined(__clang__) || defined(__INTEL_COMPILER)
    *  ||(__GNUC_PREREQ(4,7) && defined(__SSE__))
    * #include <xmmintrin.h>
    * #define plasma_spin_pause()  _mm_pause()
    * gcc 4.7 xmmintrin.h defines _mm_pause() as rep; nop, which is portable to
    * chips pre-Pentium 4.  Although opcode generated is same, we want pause
    * (if pause not supported by older x86 assembler, "rep; nop" is equivalent)
    * #define plasma_spin_pause()  __asm__ __volatile__ ("rep; nop") */
  #define plasma_spin_pause()  __asm__ __volatile__ ("pause")

#elif defined(__ia64__)

  /* http://www.intel.com/content/www/us/en/processors/itanium/itanium-architecture-vol-3-manual.html  volume 3 page 145 (3:145) */
  #if (defined(__HP_cc__) || defined(__HP_aCC__))
    #define plasma_spin_pause()  _Asm_hint(_HINT_PAUSE)
  #else
    #define plasma_spin_pause()  __asm__ __volatile__ ("hint @pause")
  #endif


#elif defined(__arm__)

  /* ARMv7 Architecture Reference Manual (for YIELD) */
  /* ARM Compiler toolchain Compiler Reference (for __yield() instrinsic) */
  #ifdef __CC_ARM
  #define plasma_spin_pause()  __yield()
  #else
  #define plasma_spin_pause()  __asm__ __volatile__ ("yield")
  #endif

#elif defined(__sparc) || defined(__sparc__)

  /* http://www.oracle.com/technetwork/systems/opensparc/sparc-architecture-2011-1728132.pdf
   *   5.5.12 Pause Count (PAUSE) Register (ASR 27)
   *   7.100 Pause
   *     PAUSE instruction
   *   7.141 Write Ancillary State Register
   *     WRPAUSE instruction
   *     wr regrs1, reg_or_imm,%pause
   *
   * See thorough DELAY_SPIN implementation in Solaris kernel:
   * https://github.com/joyent/illumos-joyent/blob/master/usr/src/common/atomic/sparcv9/atomic.s
   * https://hg.openindiana.org/upstream/illumos/illumos-gate/rev/7c80b70bb8de
   *
   * Simpler:
   * https://github.com/joyent/illumos-joyent/blob/master/usr/src/lib/libc/capabilities/sun4v/common/smt_pause.s
   */
  #if 0  /* barrier only prevents compiler from optimizing away a spin loop */
  #if (defined(__SUNPRO_C)  && __SUNPRO_C  >= 0x5110) \
   || (defined(__SUNPRO_CC) && __SUNPRO_CC >= 0x5110)
  /* http://www.oracle.com/technetwork/server-storage/solarisstudio/documentation/oss-compiler-barriers-176055.pdf
   * http://www.oracle.com/technetwork/server-storage/solarisstudio/documentation/oss-memory-barriers-fences-176056.pdf */
  #include <mbarrier.h>
  #define plasma_spin_pause()  __compiler_barrier()
  #else
  #define plasma_spin_pause()  __asm __volatile__("membar #LoadLoad":::"memory")
  #endif
  #endif /* barrier only prevents compiler from optimizing away a spin loop */

  /* Note: SPARC-T4 and newer come with a 'pause' instruction.
   * The the length of the pause can be modified by writing to ASR 27
   *   e.g.  wr %%g0, 128, %%asr27
   * However, for compatibility with all SPARC, do a generic pause by reading
   * the condition code register a few times.  If instead of inline assembly,
   * this was implemented as a routine in a shared library, then a function
   * optimized for the specific architecture and model could be used */
  #if defined(__SUNPRO_C)
    #define plasma_spin_pause()  __asm __volatile__ ("rd %%ccr, %%g0 \n\t" \
                                                     "rd %%ccr, %%g0 \n\t" \
                                                     "rd %%ccr, %%g0")
  #else /* defined(__SUNPRO_CC) || defined(__GNUC__) */
    #define plasma_spin_pause()  __asm __volatile__ ("rd %ccr, %g0 \n\t" \
                                                     "rd %ccr, %g0 \n\t" \
                                                     "rd %ccr, %g0")
  #endif

#elif defined(__ppc__)   || defined(_ARCH_PPC)  || \
      defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER)

  /* http://stackoverflow.com/questions/5425506/equivalent-of-x86-pause-instruction-for-ppc
   * https://www.power.org/documentation/power-instruction-set-architecture-version-2-06/attachment/powerisa_v2-06b_v2_public/  (requires free account)
   * 3.2 "or" Instruction
   *   "or" Shared Resource Hints
   * For compatibility use 'or 27,27,27' instead of 'yield' extended mnemonic
   * (available with POWER 7) and use volatile instead of __volatile__ since
   * -qlanglvl=stdc99 rejects __volatile__, though -qlanglvl=extc99 supports it)
   */
  #define plasma_spin_pause()  __asm__ volatile ("or 27,27,27")

#else

  /* help prevent compiler from optimizing loop away; not a pause */
  #define plasma_spin_pause()  plasma_membar_ccfence()

#endif


/* plasma_spin_yield()
 *   attempt switch to another thread on same CPU, else sleep 1ms */

#if defined(_MSC_VER)
  /* prototype defined in different headers in different MS Windows versions */
  VOID WINAPI Sleep(_In_ DWORD dwMilliseconds);
  BOOL WINAPI SwitchToThread(void);
  #define plasma_spin_yield() do {if (!SwitchToThread()) Sleep(1);} while (0)
#elif defined(PLASMA_FEATURE_POSIX)
  #include <unistd.h>  /* _POSIX_PRIORITY_SCHEDULING */
  #include <poll.h>    /* might also need _XOPEN_SOURCE=500 or better on _AIX */
  #if defined(_POSIX_PRIORITY_SCHEDULING) && (_POSIX_PRIORITY_SCHEDULING-0)!= -1
  #include <sched.h>
  #define plasma_spin_yield() do {if (sched_yield()!=0) poll(NULL,0,1);}while(0)
  #else
  #define plasma_spin_yield() poll(NULL, 0, 1)
  #endif
#endif

#if defined(__APPLE__) && defined(__MACH__)
  /* use Mach kernel thread_switch() to reduce priority inversion spinning
   * http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/thread_switch.html
   * http://www.gnu.org/software/hurd/gnumach-doc/Hand_002dOff-Scheduling.html
   * http://stackoverflow.com/questions/12949028/spin-lock-implementations-osspinlock?rq=1
   */
  #include <TargetConditionals.h>
  #if TARGET_OS_MAC
    #include <mach/thread_switch.h>
    #undef  plasma_spin_yield
    #define plasma_spin_yield() \
    do {if (thread_switch(THREAD_NULL,SWITCH_OPTION_DEPRESS,1) != KERN_SUCCESS \
            && sched_yield() != 0) \
            poll(NULL, 0, 1); \
    } while (0) /* (untested!) */
  #endif
#endif


#ifdef __cplusplus
extern "C" {
#endif


/* plasma_spin_lock_init()
 * plasma_spin_lock_acquire()
 * plasma_spin_lock_acquire_try()
 * plasma_spin_lock_acquire_spinloop() 
 * plasma_spin_lock_acquire_spindecay() 
 * plasma_spin_lock_release()
 *
 * plasma_spin_lock_acquire() is intended for use protecting small critical
 * sections that do not contain any (zero, zilch, nada) blocking calls,
 * e.g. for use when the cost of obtaining a mutex or the cost of a context
 * switch is greater than the cost of executing the instructions in the
 * critical section.
 *
 * plasma_spin_lock_acquire_spindecay() 
 *   spin loop hybrid with adjustable spin counts for levels of backoff/decay
 *     (pause count, (pause x 32) count, yield count)
 *     e.g. plasma_spin_lock_acquire_spindecay(spin, 32, 64, 8)
 *   returns true if lock obtained, false if backoff counts exhausted (decayed)
 * - again, caller should avoid blocking operations inside critical section
 * - for potential use with moderately contended locks (YMMV)
 * - for contended cases after measuring this does better than basic spinloop
 * - adjust spin count to your needs after testing your application
 * Note that if spin often reaches yield level without obtaining lock, then 
 * performance suffers and a real mutex is probably a better choice.  Though
 * yielding the CPU aims to prevent continual spin for long periods of time
 * when something else is wrong, spinning fruitlessly is still expensive and
 * many threads spinning results in fewer resources available for real work.
 *
 * Performance notes:
 * - locks should be spread out in memory and not share same cache line
 *   (to avoid false sharing); recommend 128-byte separation
 *   recommend adding padding on both sides of lock structure when allocating
 *   (again, to avoid false sharing), especially if lock struct is on the stack
 * - not bothering to do non-atomic pre-check of spin->lck prior to atomic
 *   attempt to acquire the lock since branch prediction and pipelining will
 *   likely result in the atomic instruction being started anyway.  When there
 *   is significant contention, it might make sense to directly call
 *   plasma_spin_lock_acquire_spinloop() or plasma_spin_lock_acquire_spindecay()
 *   instead of plasma_spin_lock_acquire()
 * - plasma_spin_lock_t lck member is intentionally not volatile
 *   (avoid compiler volatile effects)
 * - prefer using real mutex for highly contended locks; OS-provided mutex are
 *   implemented as adaptive mutex in most modern operating system standard libs
 *   (uncontended mutexes are locked in user-space, and contended mutexes spin
 *   in user-space for a short period before falling back to the kernel to
 *   arbitrate) (other options include semaphores, wait queues, event objects)
 * - prefer Apple OSSpinLock* implementation where available
 *   On uniprocessor systems, spinlock implementation does not needlessly spin.
 *   (A sizable number of mobile devices are uniprocessor and single core.)
 */


#if (defined(__APPLE__) && defined(__MACH__)) \
 && defined(MAC_OS_X_VERSION_MIN_REQUIRED) \
 && MAC_OS_X_VERSION_MIN_REQUIRED-0 >= 1070   /* OSSpinLock in OSX 10.7 */
#define plasma_spin_lock_init(spin) \
        ((spin)->lck=OS_SPINLOCK_INIT, (spin)->udata32=0, (spin)->udata64=0)
#define PLASMA_SPIN_LOCK_INITIALIZER           { OS_SPINLOCK_INIT, 0, 0 }
#define plasma_spin_lock_release(spin)           OSSpinLockUnlock(&(spin)->lck)
#define plasma_spin_lock_acquire_try(spin)       OSSpinLockTry(&(spin)->lck)
#define plasma_spin_lock_acquire(spin)           OSSpinLockLock(&(spin)->lck)
#define plasma_spin_lock_acquire_spinloop(spin)  OSSpinLockLock(&(spin)->lck)
/* (OSSpinLock currently defined by libkern/OSAtomic.h as int32_t) */
typedef OSSpinLock plasma_spin_lock_lock_t;
#else
typedef uint32_t plasma_spin_lock_lock_t;
#endif


typedef __attribute_aligned__(16)
struct plasma_spin_lock_t {
    plasma_spin_lock_lock_t lck;
    uint32_t udata32; /* user data 4-bytes */
    uint64_t udata64; /* user data 8-bytes */
} plasma_spin_lock_t;

/* default spin lock macro implementation */
#ifndef PLASMA_SPIN_LOCK_INITIALIZER

#ifdef __STDC_C99
#define PLASMA_SPIN_LOCK_INITIALIZER { .lck = PLASMA_ATOMIC_LOCK_INITIALIZER, \
                                       .udata32 = 0, \
                                       .udata64 = 0 }
#else
#define PLASMA_SPIN_LOCK_INITIALIZER { PLASMA_ATOMIC_LOCK_INITIALIZER, 0, 0 }
#endif

#define plasma_spin_lock_init(spin) \
        ((spin)->lck = PLASMA_ATOMIC_LOCK_INITIALIZER, \
         (spin)->udata32 = 0, \
         (spin)->udata64 = 0)

#define plasma_spin_lock_release(spin) \
        plasma_atomic_lock_release(&(spin)->lck)

#define plasma_spin_lock_acquire_try(spin) \
        plasma_atomic_lock_acquire(&(spin)->lck)

#define plasma_spin_lock_acquire(spin) \
       (plasma_spin_lock_acquire_try(spin) \
        || plasma_spin_lock_acquire_spinloop(spin))

#endif /* default spin lock macro implementation */


#ifndef plasma_spin_lock_acquire_spinloop
/*(plasma_spin_lock_acquire_spinloop() always returns true)*/
bool
plasma_spin_lock_acquire_spinloop (plasma_spin_lock_t * const spin)
  __attribute_nonnull__;
#endif

bool
plasma_spin_lock_acquire_spindecay (plasma_spin_lock_t * const spin,
                                    int pause1, int pause32, int yield)
  __attribute_nonnull__;


/* plasma_spin_tktlock_*()  ticket lock
 * (see bottom of file for ticket lock references)
 *
 * plasma_spin_tktlock_init()
 * plasma_spin_tktlock_is_free()
 * plasma_spin_tktlock_acquire()
 * plasma_spin_tktlock_acquire_spinloop() 
 * plasma_spin_tktlock_release()
 * 
 * Note: no trylock interface for ticket lock; trylock can be approximated by
 * checking plasma_spin_tktlock_is_free() prior to attempting to obtain lock,
 * but doing so might result in two cache line misses to obtain lock, similar
 * to CAS.
 */

typedef __attribute_aligned__(16)
struct plasma_spin_tktlock_t {
    union { uint32_t u; struct { uint16_t le; uint16_t be; } t; } lck;
    uint32_t udata32; /* user data 4-bytes */
    uint64_t udata64; /* user data 8-bytes */
} plasma_spin_tktlock_t;

#define PLASMA_SPIN_TKTLOCK_TKTMAX       0xFFFFu
#define PLASMA_SPIN_TKTLOCK_TKTINC       0x10000u
#define PLASMA_SPIN_TKTLOCK_MASK(x)      ((x) &  PLASMA_SPIN_TKTLOCK_TKTMAX)
#define PLASMA_SPIN_TKTLOCK_SHIFT(x)     ((x) >> 16)

#ifdef __STDC_C99
#define PLASMA_SPIN_TKTLOCK_INITIALIZER {.lck.u = 0, .udata32 = 0, .udata64 = 0}
#else
#define PLASMA_SPIN_TKTLOCK_INITIALIZER { { 0 }, 0, 0 }
#endif
#define plasma_spin_tktlock_init(t) ((t)->lck.u=0,(t)->udata32=0,(t)->udata64=0)

PLASMA_SPIN_C99INLINE
bool
plasma_spin_tktlock_is_free (const plasma_spin_tktlock_t * const restrict spin)
  __attribute_nonnull__;
#ifdef PLASMA_SPIN_C99INLINE_FUNCS
PLASMA_SPIN_C99INLINE
bool
plasma_spin_tktlock_is_free (const plasma_spin_tktlock_t * const restrict spin)
{
    const uint32_t tkt = spin->lck.u;
    return (PLASMA_SPIN_TKTLOCK_SHIFT(tkt) == PLASMA_SPIN_TKTLOCK_MASK(tkt));
}
#endif

/*(plasma_spin_tktlock_acquire_spinloop() always returns true)*/
__attribute_noinline__
bool
plasma_spin_tktlock_acquire_spinloop (plasma_spin_tktlock_t *
                                        const restrict spin, uint32_t tkt)
  __attribute_nonnull__;

/*(plasma_spin_tktlock_acquire() always returns true)*/
PLASMA_SPIN_C99INLINE
bool
plasma_spin_tktlock_acquire (plasma_spin_tktlock_t * const restrict spin)
  __attribute_nonnull__;
#ifdef PLASMA_SPIN_C99INLINE_FUNCS
PLASMA_SPIN_C99INLINE
bool
plasma_spin_tktlock_acquire (plasma_spin_tktlock_t * const restrict spin)
{
    const uint32_t tkt = /* increment ticket count (high 16-bits of lck) */
      plasma_atomic_fetch_add_u32(&spin->lck.u, PLASMA_SPIN_TKTLOCK_TKTINC,
                                  memory_order_relaxed);
    if (__builtin_expect(
          (PLASMA_SPIN_TKTLOCK_SHIFT(tkt)==PLASMA_SPIN_TKTLOCK_MASK(tkt)), 1)) {
        /* (atomic rmw op provides acquire barrier on some platforms (e.g. x86);
         *  emit barrier only if platform requires barrier after rmw atomic)
         * (not sure if atomic_thread_fence(memory_order_acq_rel) does same)*/
        plasma_membar_atomic_thread_fence_acq_rel();
        return true;
    }
    return plasma_spin_tktlock_acquire_spinloop(spin, tkt);
}
#endif

PLASMA_SPIN_C99INLINE
void
plasma_spin_tktlock_release (plasma_spin_tktlock_t * const restrict spin)
  __attribute_nonnull__;
#ifdef PLASMA_SPIN_C99INLINE_FUNCS
PLASMA_SPIN_C99INLINE
void
plasma_spin_tktlock_release (plasma_spin_tktlock_t * const restrict spin)
{
    /* increment ticket num ready to be served (low 16-bits of lck) */
    /* must write only 16 bits (and avoid more expensive atomic exchange);
     * use union to avoid type-punning pointer between uint32_t and uint16_t
     * (writing then reading different members of union is unspecified in C std)
     * In C99, Annex J (non-normative) "J.1 Unspecified behavior. The following
     * are unspecified: The value of a union member other than the last one 
     * stored into (6.2.6.1)."  However, the result appears predictable in
     * practice, as long as the structure is naturally aligned and naturally
     * packed without padding.  Also, note that reading the 16-bit value,
     * incrementing it, and storing it is not a race condition with
     * plasma_spin_tktlock_acquire() -- which does 32-bit atomic add, including
     * 16 bits modified here -- since acquire does not modify these 16 bits.
     * NB: must test behavior with different compiler optimization levels and
     * ensure that pointer aliasing in union is not optimized into incorrect
     * code.  Might need to disable optimization on this routine or set
     * compiler optimization fences if atomic store does not provide barrier */
    __attribute_may_alias__
  #if defined(__LITTLE_ENDIAN__)
    uint16_t * const ptr = &spin->lck.t.le;
  #elif defined(__BIG_ENDIAN__)
    uint16_t * const ptr = &spin->lck.t.be;
  #endif
    const uint16_t n = *ptr + (uint16_t)1u;
    plasma_atomic_store_explicit(ptr, n, memory_order_release);
}
#endif


/* plasma_spin_taglock_*() (fuzzy ticket lock)
 * (alternative to strict ticket lock)
 * 
 * Traditional ticket lock is fair, with each waiting thread acquiring the lock
 * in strict progression.  However, when a ticket lock is released and the count
 * incremented, the thread waiting for that ticket number might be sleeping,
 * which delays everyone.  plasma_spin_taglock_*() provides an alternative.
 * With a small number of threads spinning for the same batch of tag numbers,
 * there is a higher probability that at least one will be actively spinning
 * and able to grab the lock without further delay, while at the same time
 * avoiding a thundering herd.  Those with tags in the batch all get the lock
 * before the next batch, so the number of waiters for the taglock converges
 * to one for each batch, maintaining some fairness, though fuzzier than
 * traditional, strictly fair ticket lock.
 *
 * Additional features:
 * - plasma_spin_taglock allows for higher priority threads to compete for the
 *   lock with the current batch, instead of going to the end of the queue,
 *   (high priority threads can use plasma_spin_taglock_acquire_urgent())
 *   (high priority threads using this might starve low priority threads)
 */

typedef __attribute_aligned__(16)
struct plasma_spin_taglock_t {
    uint32_t lck;
    uint32_t tag;
    uint64_t udata64; /* user data 8-bytes */
} plasma_spin_taglock_t;

#define PLASMA_SPIN_TAGLOCK_TAGMAX       0x7FFFFFFFu
#define PLASMA_SPIN_TAGLOCK_ORVAL        0x80000000u
#define PLASMA_SPIN_TAGLOCK_MASK(x)      ((x) &  PLASMA_SPIN_TAGLOCK_TAGMAX)
#define PLASMA_SPIN_TAGLOCK_IS_LOCKED(x) ((x) &  PLASMA_SPIN_TAGLOCK_ORVAL)
#define PLASMA_SPIN_TAGLOCK_UNLOCKVAL(x) ((x) & ~PLASMA_SPIN_TAGLOCK_ORVAL)
#define PLASMA_SPIN_TAGLOCK_LOCKVAL(x)   ((x) |  PLASMA_SPIN_TAGLOCK_ORVAL)

#ifdef __STDC_C99
#define PLASMA_SPIN_TAGLOCK_INITIALIZER { .lck = 0, .tag = 0, .udata64 = 0 }
#else
#define PLASMA_SPIN_TAGLOCK_INITIALIZER { 0, 0, 0 }
#endif
#define plasma_spin_taglock_init(t) ((t)->lck=0, (t)->tag=0, (t)->udata64=0)

#define plasma_spin_taglock_is_free(taglock) \
        (!PLASMA_SPIN_TAGLOCK_IS_LOCKED((taglock)->lck))

#define plasma_spin_taglock_is_contended(taglock) \
        ((taglock)->lck != PLASMA_SPIN_TAGLOCK_MASK((taglock)->tag))

#ifndef __ia64__
#define PLASMA_SPIN_TAGLOCK_VIA_FETCH_OR
#endif

/* NOTE: plasma_spin_taglock_acquire_try() is *unfair*; avoid under contention
 * (e.g. caller might first check lock using plasma_spin_taglock_is_contended()
 *  or maintain a flag in separate cache line to call acquire vs acquire_try) */
PLASMA_SPIN_C99INLINE
bool
plasma_spin_taglock_acquire_try (plasma_spin_taglock_t * const restrict taglock)
  __attribute_nonnull__;
#ifdef PLASMA_SPIN_C99INLINE_FUNCS
PLASMA_SPIN_C99INLINE
bool
plasma_spin_taglock_acquire_try (plasma_spin_taglock_t * const restrict taglock)
{
  #ifdef PLASMA_SPIN_TAGLOCK_VIA_FETCH_OR
    uint32_t * const restrict lck = (uint32_t *)&taglock->lck;
    const uint32_t cmp =
      plasma_atomic_fetch_or_u32(lck, PLASMA_SPIN_TAGLOCK_ORVAL,
                                 memory_order_relaxed);
    if (!PLASMA_SPIN_TAGLOCK_IS_LOCKED(cmp)) {
        /* jumped queue by locking without incrementing tag
         * (so decrement lck since taglock_release will increment lck) */
        *lck = PLASMA_SPIN_TAGLOCK_LOCKVAL(cmp-1);
        plasma_membar_atomic_thread_fence_acq_rel();
        return true;
    }
    return false;
  #else /* Must read prior to CAS, so check if contended before CAS attempt */
    uint32_t * const restrict lck = (uint32_t *)&taglock->lck;
    const uint32_t cmp = *lck;/* bypass queue if not locked and not contended */
    if (cmp == PLASMA_SPIN_TAGLOCK_MASK(taglock->tag)/*(jump queue:(cmp - 1))*/
        && plasma_atomic_CAS_32(lck,cmp,PLASMA_SPIN_TAGLOCK_LOCKVAL(cmp - 1))) {
        plasma_membar_atomic_thread_fence_acq_rel();
        return true;
    }
    return false;
  #endif
}
#endif


bool
plasma_spin_taglock_acquire (plasma_spin_taglock_t * const restrict taglock)
  __attribute_nonnull__;

/* NOTE: plasma_spin_taglock_acquire_urgent() is *unfair*
 * (but that is the point); avoid many urgent contenders */
bool
plasma_spin_taglock_acquire_urgent (plasma_spin_taglock_t *
                                      const restrict taglock)
  __attribute_nonnull__;


PLASMA_SPIN_C99INLINE
void
plasma_spin_taglock_release (plasma_spin_taglock_t * const restrict taglock)
  __attribute_nonnull__;
#ifdef PLASMA_SPIN_C99INLINE_FUNCS
PLASMA_SPIN_C99INLINE
void
plasma_spin_taglock_release (plasma_spin_taglock_t * const restrict taglock)
{
    uint32_t * const restrict lck = &taglock->lck;
    plasma_atomic_store_explicit(lck, PLASMA_SPIN_TAGLOCK_UNLOCKVAL(1u+*lck),
                                 memory_order_release);
}
#endif


#ifdef __cplusplus
}
#endif

#endif




/* NOTES and REFERENCES
 *
 * Discussions about spinning and performance:
 *
 * http://www.1024cores.net/home/lock-free-algorithms/tricks/spinning
 * http://software.intel.com/en-us/articles/implementing-scalable-atomic-locks-for-multi-core-intel-em64t-and-ia32-architectures/
 * http://stackoverflow.com/questions/11923151/is-there-any-simple-way-to-improve-performance-of-this-spinlock-function
 * http://stackoverflow.com/questions/11959374/fastest-inline-assembly-spinlock
 * http://locklessinc.com/articles/priority_locks/
 * http://locklessinc.com/articles/locks/
 * http://www.embedded.com/electronics-blogs/beginner-s-corner/4023947/Introduction-to-Priority-Inversion
 *
 * http://www.1024cores.net/home/lock-free-algorithms/tricks/spinning
 *   "On Linux passive spinning is implemented with pthread_yield(), or nanosleep() for "deeper" spin."  [Ed: pthread_yield() not portable; sched_yield() is]
 *   "On Windows situation is a bit more involved. There is SwitchToThread() which is limited to the current processor. There is Sleep(0) which is limited to threads of no-less priority (it's unclear as to whether it's intra-processor or inter-processor). Finally, there is Sleep(1) which should cover all cases (all priorities/all processors)."
 *    [See section Hybrid spinning for sample do_backoff() routine]
 *
 * http://cbloomrants.blogspot.com/2011/07/07-29-11-spinning.html
 *   "Obviously a good software engineer doesn't just randomly stick calls to STT or whatever in their spin loops, they make a function like MySpinYield() and call their function so that they can encapsulate the tricky platform-specific knowledge in one place. It just staggers me how rarely I see anyone do things like this.
 *   "But really you shouldn't EVER do a spin-wait on Windows. There's no reason for it. Any place you are tempted to use a spin-wait, use an "eventcount" instead and do a proper wait. (perhaps after a few spins, but only if you have very good reason to believe those spins actually help)
 *   "BTW I've written about the balanced set manager in detail here :
http://cbloomrants.blogspot.com/2010/09/09-12-10-defficiency-of-windows-multi.html "
 *
 * SwitchToThread()
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms686352%28v=vs.85%29.aspx
 *
 * Sleep(1)
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms686298%28v=vs.85%29.aspx
 * http://www.bluebytesoftware.com/blog/2006/08/23/PriorityinducedStarvationWhySleep1IsBetterThanSleep0AndTheWindowsBalanceSetManager.aspx
 *
 * timeBeginPeriod(1)  (set default system quantum to 1ms instead of 10-15ms)
 * (generally not a good idea to do this for extended periods of time)
 * http://msdn.microsoft.com/en-us/library/windows/desktop/dd757624%28v=vs.85%29.aspx
 * http://cbloomrants.blogspot.com/2010/09/09-12-10-defficiency-of-windows-multi.html
 * http://msdn.microsoft.com/en-us/library/ms713413%28v=vs.85%29.aspx
 * http://stackoverflow.com/questions/7590475/why-does-increasing-timer-resolution-via-timebeginperiod-impact-power-consumptio
 * http://stackoverflow.com/questions/7685762/windows-7-timing-functions-how-to-use-getsystemtimeadjustment-correctly/11743614#11743614
 * Timers, Timer Resolution, and Development of Efficient Code
 * http://msdn.microsoft.com/en-us/windows/hardware/gg463266.aspx
 *
 * Apple OSAtomic.h Reference
 * https://developer.apple.com/library/mac/#documentation/System/Reference/OSAtomic_header_reference/Reference/reference.html#//apple_ref/doc/uid/TP40011482
 *
 * Ticket lock
 * http://en.wikipedia.org/wiki/Ticket_lock
 * http://en.wikipedia.org/wiki/Fetch-and-add
 * http://lwn.net/Articles/267968/   (including comments)
 * http://www.intel.com/content/dam/www/public/us/en/documents/white-papers/xeon-lock-scaling-analysis-paper.pdf
 */
