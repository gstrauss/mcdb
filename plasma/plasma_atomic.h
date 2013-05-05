/*
 * plasma_atomic - portable macros for processor-specific atomic operations
 *
 *   inline assembly and compiler intrinsics for atomic operations
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

#ifndef INCLUDED_PLASMA_ATOMIC_H
#define INCLUDED_PLASMA_ATOMIC_H

#include <inttypes.h>
#include "plasma_attr.h"
#include "plasma_membar.h"

/*
 * plasma_atomic_lock_acquire - basic lock providing acquire semantics
 * plasma_atomic_lock_release - basic unlock providing release semantics
 * plasma_atomic_lock_init (or PLASMA_ATOMIC_LOCK_INITIALIZER)
 *
 * NB: unlike pthread mutex, plasma_atomic_lock_acquire() provides acquire
 * semantics and -not- sequential consistency (which would include StoreLoad)
 *
 * plasma_atomic_CAS_ptr - atomic pointer compare-and-swap (CAS)
 * plasma_atomic_CAS_64  - atomic 64-bit  compare-and-swap (CAS)
 * plasma_atomic_CAS_32  - atomic 32-bit  compare-and-swap (CAS)
 *
 * NB: Intended use is with regular, cacheable memory
 *     Not appropriate for MMIO, SSE, or drivers
 *     Futher information can be found in plasma_membar.h
 *
 * NB: please be aware how these building blocks are used
 *     platforms with CAS instruction are vulnerable to A-B-A race
 *       when swapping ptrs to structs and modifying the values in the structs
 *       (e.g. next ptr in struct) -- caller must take appropriate precautions
 *       (CAS == compare-and-swap)
 *     platforms with LL/SC instructions avoid A-B-A race
 *       (LL/SC == load linked, store conditional)
 *
 * NB: Input must be properly aligned for atomics to function properly
 *     64-bit must be at least 8-byte aligned
 *     32-bit must be at least 4-byte aligned
 *
 * NB: Use plasma_atomic_CAS_ptr() instead of plasma_atomic_CAS_ptr_impl()
 *     Use plasma_atomic_CAS_64()  instead of plasma_atomic_CAS_64_impl()
 *     Use plasma_atomic_CAS_32()  instead of plasma_atomic_CAS_32_impl()
 *
 *     plasma_atomic_CAS_*_impl() is intended to be used in inline functions
 *     which comply with the following restrictions: cmpval must be a variable
 *     on the stack (and not a constant), must be re-initialized each time
 *     before these macros are called, and must not have side effects of
 *     evaluation.  This is so that when these macros are used in reentrant
 *     code, there is not a race condition accessing cmpval more than once.
 *     There would be a race if cmpval were accessed through a pointer that
 *     another thread might modify.  Additionally, implementations might modify
 *     cmpval, another reason cmpval must be re-initialized before each use,
 *     and why cmpval must be a variable and not a constant.  Typical usage
 *     of these macros is in a loop, and cmpval should already need to be
 *     re-initialized before each use for many scenarios using these macros.
 *     To avoid compiler warnings, casts are used, so type-safety is discarded.
 *
 * NB: Note: 64-bit ops might not be available when code is compiled 32-bits.
 *     (depends on the platform and compiler intrinsic support)
 *
 * NB: in contrast to C11 pattern where atomic_xxx() implies sequential
 *     consistency and atomic_xxx_explicit() specifies memory_order,
 *     plasma_atomic_xxx_T(), where T is type, implies memory_order_relaxed
 *     (i.e. no barrier), and plasma_atomic_xxx_T_mmm species memory_order.
 *
 * FUTURE: Detect and employ C11 atomics where available
 * FUTURE: Current plasma_atomic implementation is done w/ compiler intrinsics.
 *         Intrinsics might provide stronger (slower) memory barriers than
 *         required.  Might provide assembly code implementations with more
 *         precise barriers and to support additional compilers on each platform
 */

#if defined(_MSC_VER)

  #include <intrin.h>
  #pragma intrinsic(_InterlockedCompareExchange)
  #pragma intrinsic(_InterlockedCompareExchange64)
  #pragma intrinsic(_InterlockedCompareExchangePointer)
  #pragma intrinsic(_InterlockedExchange)
  #pragma intrinsic(_InterlockedExchange64)
  #pragma intrinsic(_InterlockedExchangePointer)

  #define plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval) \
          (_InterlockedCompareExchangePointer((ptr),(newval),(cmpval)) \
                                                                  == (cmpval))
  #define plasma_atomic_CAS_64_impl(ptr, cmpval, newval) \
          (_InterlockedCompareExchange64((ptr),(newval),(cmpval)) == (cmpval))
  #define plasma_atomic_CAS_32_impl(ptr, cmpval, newval) \
          (_InterlockedCompareExchange((ptr),(newval),(cmpval)) == (cmpval))
  #define plasma_atomic_xchg_ptr_impl(ptr, newval) \
          _InterlockedExchangePointer((ptr),(newval))
  #define plasma_atomic_xchg_64_impl(ptr, newval) \
          _InterlockedExchange64((ptr),(newval))
  #define plasma_atomic_xchg_32_impl(ptr, newval) \
          _InterlockedExchange((ptr),(newval))

#elif defined(__APPLE__)

  #include <libkern/OSAtomic.h>
  #if (   (defined(MAC_OS_X_VERSION_MIN_REQUIRED) \
           && MAC_OS_X_VERSION_MIN_REQUIRED-0 >= 1050) \
       || (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) \
           && __IPHONE_OS_VERSION_MIN_REQUIRED-0 >= 20000)   )
  #define plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval) \
          (OSAtomicCompareAndSwapPtr((cmpval),(newval),(ptr)))
  #endif
  #define plasma_atomic_CAS_64_impl(ptr, cmpval, newval) \
          (OSAtomicCompareAndSwap64((int64_t)(cmpval),(int64_t)(newval), \
                                    (int64_t *)(ptr)))
  #define plasma_atomic_CAS_32_impl(ptr, cmpval, newval) \
          (OSAtomicCompareAndSwap32((int32_t)(cmpval),(int32_t)(newval), \
                                    (int32_t *)(ptr)))

#elif defined(__sun)

  #include <atomic.h>
  #define plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval) \
          (atomic_cas_ptr((ptr),(cmpval),(newval)) == (cmpval))
  #define plasma_atomic_CAS_64_impl(ptr, cmpval, newval) \
          (atomic_cas_64((ptr),(cmpval),(newval)) == (cmpval))
  #define plasma_atomic_CAS_32_impl(ptr, cmpval, newval) \
          (atomic_cas_32((ptr),(cmpval),(newval)) == (cmpval))
  #define plasma_atomic_xchg_ptr_impl(ptr, newval) \
          (atomic_swap_ptr((ptr),(newval)))
  #define plasma_atomic_xchg_64_impl(ptr, newval) \
          (atomic_swap_64((ptr),(newval)))
  #define plasma_atomic_xchg_32_impl(ptr, newval) \
          (atomic_swap_32((ptr),(newval)))

#elif (defined(__ppc__)   || defined(_ARCH_PPC)  || \
       defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER)) \
   && ((defined(__IBMC__) || defined(__IBMCPP__)) || !defined(__GNUC__))

  /* AIX xlC compiler intrinsics __compare_and_swap() and __compare_and_swaplp()
   * require pointer to cmpval and the original contents in ptr is always
   * copied into cmpval, whether or not the swap occurred.
   * (Be sure to save/reset cmpval if used in loop)
   * AIX xlC compiler intrinsics __check_lock_mp() and __check_lockd_mp() do not
   * require a pointer to cmpval, but note that the return value is the inverse
   * to the return value of __compare_and_swap()
   * AIX xlC compiler intrinsics __clear_lock_mp() and __clear_lockd_mp() add a
   * full 'sync' barrier */
  #include <sys/atomic_op.h>
  #if defined(__IBMC__) || defined(__IBMCPP__)
    #define plasma_atomic_CAS_64_impl(ptr, cmpval, newval) \
            __compare_and_swaplp((ptr),&(cmpval),(newval))
            /* !__check_lockd_mp((ptr),(cmpval),(newval)) */
    #define plasma_atomic_CAS_32_impl(ptr, cmpval, newval) \
            __compare_and_swap((ptr),&(cmpval),(newval))
            /* !__check_lock_mp((ptr),(cmpval),(newval)) */
    #define plasma_atomic_xchg_64_impl(ptr, newval) \
            __fetch_and_swaplp((ptr),(newval))
    #define plasma_atomic_xchg_32_impl(ptr, newval) \
            __fetch_and_swap((ptr),(newval))
  #else
    #define plasma_atomic_CAS_64_impl(ptr, cmpval, newval) \
            compare_and_swaplp((ptr),&(cmpval),(newval))
    #define plasma_atomic_CAS_32_impl(ptr, cmpval, newval) \
            compare_and_swap((ptr),&(cmpval),(newval))
            /* !_check_lock((ptr),(cmpval),(newval)) */
  #endif

#elif defined(__ia64__) \
  && (defined(__HP_cc__) || defined(__HP_aCC__))

  #include <machine/sys/inline.h>
  /* Implementing Spinlocks on Itanium and PA-RISC
   * Section 3.3.1 Compare-and-Exchange */
  #define plasma_atomic_ia64_relaxed_fence \
    (_Asm_fence) (  _UP_CALL_FENCE | _UP_SYS_FENCE | \
                  _DOWN_CALL_FENCE | _DOWN_SYS_FENCE )
  #define plasma_atomic_CAS_64_impl(ptr, cmpval, newval) \
          (_Asm_mov_to_ar(_AREG_CCV,(cmpval),plasma_atomic_ia64_relaxed_fence),\
           _Asm_cmpxchg(_SZ_D,_SEM_ACQ,(ptr),(newval),_LDHINT_NONE) == (cmpval))
  #define plasma_atomic_CAS_32_impl(ptr, cmpval, newval) \
          (_Asm_mov_to_ar(_AREG_CCV,(cmpval),plasma_atomic_ia64_relaxed_fence),\
           _Asm_cmpxchg(_SZ_W,_SEM_ACQ,(ptr),(newval),_LDHINT_NONE) == (cmpval))

  /* Microsoft has stopped supporting Windows on Itanium and
   * RedHat has stopped supporting Linux on Itanium, so even though Linux on
   * Itanium is still viable, further support is omitted here, at least for now.
   * Intrinsics are available for Intel icc compiler, but not implemented here*/

#elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)

  /* (note: we assume __sync_* builtins provide compiler optimization fence) */
  #define plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval) \
          __sync_bool_compare_and_swap((ptr), (cmpval), (newval))
  #define plasma_atomic_CAS_64_impl(ptr, cmpval, newval) \
          __sync_bool_compare_and_swap((ptr), (cmpval), (newval))
  #define plasma_atomic_CAS_32_impl(ptr, cmpval, newval) \
          __sync_bool_compare_and_swap((ptr), (cmpval), (newval))

#elif !defined(PLASMA_ATOMIC_MUTEX_FALLBACK)
  /* (mutex-based fallback implementation enabled by preprocessor define) */

  #error "plasma atomics not implemented for platform+compiler; suggestions?"

#endif

/* default plasma_atomic_CAS_ptr_impl() (if not previously defined) */
#ifndef plasma_atomic_CAS_ptr_impl
  #if defined(_LP64) || defined(__LP64__)  /* 64-bit */
    #define plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval) \
            plasma_atomic_CAS_64_impl((uint64_t *)(ptr), \
                                      (uint64_t)(cmpval),(uint64_t)(newval))
  #else
    #define plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval) \
            plasma_atomic_CAS_32_impl((uint32_t *)(ptr), \
                                      (uint32_t)(cmpval),(uint32_t)(newval))
  #endif
#endif


/* op sequences/combinations which provide barriers
 * (each macro is not necessarily defined for each platforms) */


/*
 * plasma_atomic_st_ptr_release - atomic pointer store with release semantics
 * plasma_atomic_st_64_release  - atomic uint64_t store with release semantics
 * plasma_atomic_st_32_release  - atomic uint32_t store with release semantics
 */

#if defined(__ia64__) || defined(_M_IA64)

  /* Itanium can extend ld and st instructions with .acq and .rel modifiers */
  #if (defined(__HP_cc__) || defined(__HP_aCC__))
    /* _Asm_st_volatile has 'release' semantics on Itanium
     * even if code compiled with +Ovolatile=__unordered */
    #define plasma_atomic_st_64_release_impl(ptr,newval) \
            _Asm_st_volatile(_SZ_D, _STHINT_NONE, (ptr), (newval))
    #define plasma_atomic_st_32_release_impl(ptr,newval) \
            _Asm_st_volatile(_SZ_W, _STHINT_NONE, (ptr), (newval))
  #else
    /* Itanium volatile variables have implicit ld.acq and st.rel semantics */
    #define plasma_atomic_st_64_release_impl(ptr,newval) \
            do { plasma_membar_ccfence(); \
                 *((volatile int64_t * restrict)(ptr)) = (int64_t)(newval); \
            } while (0)
    #define plasma_atomic_st_32_release_impl(ptr,newval) \
            do { plasma_membar_ccfence(); \
                 *((volatile int32_t * restrict)(ptr)) = (int32_t)(newval); \
            } while (0)
  #endif
  #if defined(_LP64) || defined(__LP64__)  /* 64-bit */
    #define plasma_atomic_st_ptr_release_impl(ptr,newval) \
            plasma_atomic_st_64_release_impl((ptr),(newval))
  #else
    #define plasma_atomic_st_ptr_release_impl(ptr,newval) \
            plasma_atomic_st_32_release_impl((ptr),(newval))
  #endif

#else

  #define plasma_atomic_st_ptr_release_impl(ptr,newval) \
          do { plasma_membar_atomic_thread_fence_release(); *(ptr) = (newval); \
          } while (0)
  #define plasma_atomic_st_64_release_impl(ptr,newval) \
          do { plasma_membar_atomic_thread_fence_release(); *(ptr) = (newval); \
          } while (0)
  #define plasma_atomic_st_32_release_impl(ptr,newval) \
          do { plasma_membar_atomic_thread_fence_release(); *(ptr) = (newval); \
          } while (0)

#endif

#define plasma_atomic_st_ptr_release(ptr,newval) \
        plasma_atomic_st_ptr_release_impl((ptr),(newval))
#define plasma_atomic_st_64_release(ptr,newval) \
        plasma_atomic_st_64_release_impl((ptr),(newval))
#define plasma_atomic_st_32_release(ptr,newval) \
        plasma_atomic_st_32_release_impl((ptr),(newval))


#if defined (_MSC_VER)

  #if defined(_M_IA64)
    #pragma intrinsic(_InterlockedExchange_acq)
    #pragma intrinsic(_InterlockedExchange64_acq)
    #pragma intrinsic(_InterlockedExchangePointer_acq)
    #define plasma_atomic_xchg_ptr_acquire_impl(ptr, newval) \
            _InterlockedExchangePointer_acq((ptr),(newval))
    #define plasma_atomic_xchg_64_acquire_impl(ptr, newval) \
            _InterlockedExchange64_acq((ptr),(newval))
    #define plasma_atomic_xchg_32_acquire_impl(ptr, newval) \
            _InterlockedExchange_acq((ptr),(newval))
  #elif defined(_M_AMD64) || defined(_M_IX86)
    /* (LOCK implied on xchg on x86 and results in full memory barrier) */
    #define plasma_atomic_xchg_ptr_acquire_impl(ptr, newval) \
            _InterlockedExchangePointer((ptr),(newval))
    #define plasma_atomic_xchg_64_acquire_impl(ptr, newval) \
            _InterlockedExchange64((ptr),(newval))
    #define plasma_atomic_xchg_32_acquire_impl(ptr, newval) \
            _InterlockedExchange((ptr),(newval))
  #endif

#elif defined(__sun)
  /* TSO (total store order) on SPARC, so acquire barrier is implicit */
  #define plasma_atomic_xchg_ptr_acquire_impl(ptr, newval) \
          plasma_atomic_xchg_ptr_impl((ptr),(newval))
  #define plasma_atomic_xchg_64_acquire_impl(ptr, newval) \
          plasma_atomic_xchg_64_impl((ptr),(newval))
  #define plasma_atomic_xchg_32_acquire_impl(ptr, newval) \
          plasma_atomic_xchg_32_impl((ptr),(newval))

#elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
  /* Note: gcc __sync_lock_test_and_set() limitation documented at
   *   http://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html
   * "Many targets have only minimal support for such locks, and do not support
   *  a full exchange operation. In this case, a target may support reduced
   *  functionality here by which the only valid value to store is the immediate
   *  constant 1. The exact value actually stored in *ptr is implementation
   *  defined."
   * Note: gcc __sync_lock_release adds mfence on x86, and is not used in 
   * plasma_atomic.h since mfence barrier is stronger than needed for release
   * barrier provided by plasma_atomic_lock_release() for cacheable memory */
  #define plasma_atomic_xchg_ptr_acquire_impl(ptr, newval) \
          __sync_lock_test_and_set((ptr), (newval))
  #define plasma_atomic_xchg_64_acquire_impl(ptr, newval) \
          __sync_lock_test_and_set((ptr), (newval))
  #define plasma_atomic_xchg_32_acquire_impl(ptr, newval) \
          __sync_lock_test_and_set((ptr), (newval))

#elif defined(__ia64__) \
   && (defined(__HP_cc__) || defined(__HP_aCC__))

  /* 'xchg' instruction has 'acquire' semantics on Itanium */
  #define plasma_atomic_xchg_64_acquire_impl(ptr,newval) \
          _Asm_xchg(_SZ_D, (ptr), (newval), _LDHINT_NONE)
  #define plasma_atomic_xchg_32_acquire_impl(ptr,newval) \
          _Asm_xchg(_SZ_W, (ptr), (newval), _LDHINT_NONE)
  #if defined(_LP64) || defined(__LP64__)  /* 64-bit */
    #define plasma_atomic_xchg_ptr_acquire_impl(ptr,newval) \
            plasma_atomic_xchg_64_acquire_impl((ptr),(newval))
  #else
    #define plasma_atomic_xchg_ptr_acquire_impl(ptr,newval) \
            plasma_atomic_xchg_32_acquire_impl((ptr),(newval))
  #endif

#endif


/* default plasma_atomic_xchg_ptr,64,32_impl() (if not previously defined) */
#if !defined(plasma_atomic_xchg_64_impl) \
  && defined(plasma_atomic_xchg_64_acquire_impl)
  #define plasma_atomic_xchg_64_impl(ptr,newval) \
          plasma_atomic_xchg_64_acquire_impl((ptr),(newval))
#endif
#if !defined(plasma_atomic_xchg_32_impl) \
  && defined(plasma_atomic_xchg_32_acquire_impl)
  #define plasma_atomic_xchg_32_impl(ptr,newval) \
          plasma_atomic_xchg_32_acquire_impl((ptr),(newval))
#endif
#ifndef plasma_atomic_xchg_ptr_impl
  #if defined(_LP64) || defined(__LP64__)  /* 64-bit */
    #ifdef plasma_atomic_xchg_64_impl
      #define plasma_atomic_xchg_ptr_impl(ptr, newval) \
              plasma_atomic_xchg_64_impl((uint64_t *)(ptr),(uint64_t)(newval))
    #endif
  #else
    #ifdef plasma_atomic_xchg_32_impl
      #define plasma_atomic_xchg_ptr_impl(ptr, newval) \
              plasma_atomic_xchg_32_impl((uint32_t *)(ptr),(uint32_t)(newval))
    #endif
  #endif
#endif



#include <stdint.h>   /* <inttypes.h> is more portable on pre-C99 systems */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
#include <stdbool.h>
#endif

#if defined(PLASMA_ATOMIC_MUTEX_FALLBACK)
  /* mutex-based fallback implementation, enabled by preprocessor define */
  bool
  plasma_atomic_CAS_64_impl (uint64_t * const ptr,
                             uint64_t cmpval, const uint64_t newval);
  bool
  plasma_atomic_CAS_32_impl (uint32_t * const ptr,
                             uint32_t cmpval, const uint32_t newval);
  #if defined(_LP64) || defined(__LP64__)  /* 64-bit */
    #define plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval) \
            plasma_atomic_CAS_64_impl((ptr),(cmpval),(newval))
  #else
    #define plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval) \
            plasma_atomic_CAS_32_impl((ptr),(cmpval),(newval))
  #endif
#endif

/* create inline routines to encapsulate
 * - avoid multiple use of macro param cmpval
 * - coerce values into registers or at least variables (instead of constants),
 *   (address of var and/or integer promotion required for some intrinsics) */

__attribute_regparm__((3))
bool  C99INLINE
plasma_atomic_CAS_ptr (void ** const ptr, void *cmpval, void * const newval)
  __attribute_nonnull_x__((1));
#if !defined(NO_C99INLINE)
__attribute_regparm__((3))
bool  C99INLINE
plasma_atomic_CAS_ptr (void ** const ptr, void *cmpval, void * const newval)
{
    return plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval);
}
#endif

__attribute_regparm__((3))
bool  C99INLINE
plasma_atomic_CAS_64 (uint64_t * const ptr,
                      uint64_t cmpval, const uint64_t newval)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
__attribute_regparm__((3))
bool  C99INLINE
plasma_atomic_CAS_64 (uint64_t * const ptr,
                      uint64_t cmpval, const uint64_t newval)
{
    return plasma_atomic_CAS_64_impl(ptr, cmpval, newval);
}
#endif

__attribute_regparm__((3))
bool  C99INLINE
plasma_atomic_CAS_32 (uint32_t * const ptr,
                      uint32_t cmpval, const uint32_t newval)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
__attribute_regparm__((3))
bool  C99INLINE
plasma_atomic_CAS_32 (uint32_t * const ptr,
                      uint32_t cmpval, const uint32_t newval)
{
    return plasma_atomic_CAS_32_impl(ptr, cmpval, newval);
}
#endif

/*(convenience to avoid compiler warnings about signed vs unsigned conversion)*/
#define plasma_atomic_CAS_vptr(ptr,cmpval,newval) \
        plasma_atomic_CAS_ptr((void **)(ptr),  \
                              (void *)(cmpval),(void *)(newval))
#define plasma_atomic_CAS_u64(ptr,cmpval,newval) \
        plasma_atomic_CAS_64((uint64_t *)(ptr),  \
                             (uint64_t)(cmpval),(uint64_t)(newval))
#define plasma_atomic_CAS_u32(ptr,cmpval,newval) \
        plasma_atomic_CAS_32((uint32_t *)(ptr),  \
                             (uint32_t)(cmpval),(uint32_t)(newval))

/* basic lock (32-bit) (0 == unlocked, 1 == locked)
 *   plasma_atomic_lock_acquire - basic lock providing acquire semantics
 *   plasma_atomic_lock_release - basic unlock providing release semantics
 *   plasma_atomic_lock_init (or PLASMA_ATOMIC_LOCK_INITIALIZER)
 * NB: unlike pthread_mutex_t, plasma_atomic_lock_acquire() provides acquire
 * semantics and -not- sequential consistency (which would include StoreLoad) */
#define PLASMA_ATOMIC_LOCK_INITIALIZER 0
#define plasma_atomic_lock_init(ptr) (*(ptr) = 0)

__attribute_regparm__((1))
void  C99INLINE
plasma_atomic_lock_release (uint32_t * const ptr)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
__attribute_regparm__((1))
void  C99INLINE
plasma_atomic_lock_release (uint32_t * const ptr)
{
    plasma_atomic_st_32_release(ptr, 0);
}
#endif

__attribute_regparm__((1))
bool  C99INLINE
plasma_atomic_lock_acquire (uint32_t * const ptr)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
__attribute_regparm__((1))
bool  C99INLINE
plasma_atomic_lock_acquire (uint32_t * const ptr)
{
  #ifdef plasma_atomic_xchg_32_acquire_impl

    /* basic lock is bi-state; xchg prior val of 0 means lock obtained */
    return !plasma_atomic_xchg_32_acquire_impl(ptr, 1);

  #else

    #ifdef plasma_atomic_xchg_32_impl
      #define plasma_atomic_lock_nobarrier(ptr) \
              !plasma_atomic_xchg_32_impl((ptr), 1)
              /* basic lock is bi-state; prior val of 0 means lock obtained */
    #else
      #define plasma_atomic_lock_nobarrier(ptr) \
              plasma_atomic_CAS_32((ptr), 0, 1)
    #endif
    /* (use separate statements; gcc does not handle __asm in comma operator) */
    /*return (plasma_atomic_lock_nobarrier(ptr)
     *        && (plasma_membar_atomic_thread_fence_acq_rel(), true));*/
    if (plasma_atomic_lock_nobarrier(ptr)) {
        plasma_membar_atomic_thread_fence_acq_rel();
        return true;
    }
    return false;

  #endif
}
#endif


/*
 * plasma_atomic_fetch_add_ptr - atomic pointer  fetch and add
 * plasma_atomic_fetch_add_u64 - atomic uint64_t fetch and add
 * plasma_atomic_fetch_add_u32 - atomic uint32_t fetch and add
 */

#if defined (_MSC_VER)

  /* NB: untested with signed types; might not wrap around at INT_MAX, INT_MIN
   * (Microsoft provides prototypes that take signed values)
   * (result might need to be cast back to original type)
   * (MS Windows is LLP64 ABI, so 'long' is 32-bits even when compiler 64-bit)*/
  #pragma intrinsic(_InterlockedExchangeAdd)
  #pragma intrinsic(_InterlockedExchangeAdd64)
  #define plasma_atomic_fetch_add_u64_impl(ptr, addval) \
          _InterlockedExchangeAdd64((__int64 *)(ptr),(__int64)(addval))
  #define plasma_atomic_fetch_add_u32_impl(ptr, addval) \
          _InterlockedExchangeAdd((long *)(ptr),(long)(addval))

#elif defined(__sun)

  #define plasma_atomic_fetch_add_ptr_impl(ptr,addval) \
          atomic_add_ptr((ptr),(ssize_t)(addval))
  #define plasma_atomic_fetch_add_u64_impl(ptr,addval) \
          atomic_add_64((uint64_t *)(ptr),(int64_t)(addval))
  #define plasma_atomic_fetch_add_u32_impl(ptr,addval) \
          atomic_add_32((uint32_t *)(ptr),(int32_t)(addval))

#elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)

  #define plasma_atomic_fetch_add_ptr_impl(ptr,addval) \
          __sync_fetch_and_add((ptr),(addval))
  #define plasma_atomic_fetch_add_u64_impl(ptr,addval) \
          __sync_fetch_and_add((ptr),(addval))
  #define plasma_atomic_fetch_add_u32_impl(ptr,addval) \
          __sync_fetch_and_add((ptr),(addval))

#elif defined(__APPLE__)

  /* Prefer above macros for __clang__ on MacOSX/iOS instead of these
   * NB: untested with signed types; might not wrap around at INT_MAX, INT_MIN
   * (Apple provides prototypes that take signed values)
   * (result might need to be cast back to original type)
   * NB: subtract addval from result in order to return the original value;
   *     OSAtomicAdd*() returns the resulting value, not the original value */
  #define plasma_atomic_fetch_add_u64_impl(ptr, addval) \
          (OSAtomicAdd64((int64_t)(addval),(int64_t *)(ptr))-(int64_t)(addval))
  #define plasma_atomic_fetch_add_u32_impl(ptr, addval) \
          (OSAtomicAdd32((int32_t)(addval),(int32_t *)(ptr))-(int32_t)(addval))

#elif defined(__ia64__) \
   && (defined(__HP_cc__) || defined(__HP_aCC__))

  /* NB: _Asm_fetchadd supports only -16, -8, -4, -1, 1, 4, 8, 16 !!!
   *                   and must be an immediate value, not a variable.
   *     If HP compiler supported something akin to GNU __builtin_constant_p(),
   *     then a macro could expand and check if addval is constant and legal,
   *     or else could fallback to call a routine which implemented the atomic
   *     add via compare and swap.  ** Not implemented here **
   *     Alternatives: implement in Itanium assembly or always use a CAS impl
   *     (possibly by holding a spinlock around the desired operation)
   * _Asm_fetchadd supports optional _Asm_fence arg for .acq or .rel modifiers
   *   (not used here) */
  #define plasma_atomic_fetch_add_u64_impl(ptr, addval) \
          _Asm_fetchadd(_FASZ_D,_SEM_REL,(ptr),(addval),_LDHINT_NONE)
  #define plasma_atomic_fetch_add_u32_impl(ptr, addval) \
          _Asm_fetchadd(_FASZ_W,_SEM_REL,(ptr),(addval),_LDHINT_NONE)
  /* FUTURE: if creating _acquire versions of these macros for use in locks
   * instead of xchg or CAS, _Asm_sem should be _SEM_ACQ instead of _SEM_REL and
   * additional (optional) _Asm_fence argument might be needed to cause the .acq
   * assembly instruction modifier to be emitted. */

#elif (defined(__ppc__)   || defined(_ARCH_PPC)  || \
       defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER)) \
   && (defined(__IBMC__) || defined(__IBMCPP__))

  /* use xlC load-linked/store-conditional intrinsics to build atomic ops
   * (xlC v12 supports GNUC-style __sync_fetch_and_add(), not earlier vers) */

  /* __ldarx() is only valid in 64-bit mode according to IBM docs, so the cast
   * to (long *) is valid.  If this macro is used in code compiled 32-bit, then
   * the cast will truncate the values.  Eliminate cast if valid in 32-bits. */
  #define plasma_atomic_fetch_add_u64_implreturn(ptr, addval)             \
    do {                                                                  \
        register uint64_t plasma_atomic_tmp;                              \
        do { plasma_atomic_tmp = (uint64_t)__ldarx((long *)(ptr))         \
        } while (__builtin_expect(                                        \
                   !(__stdcx((long *)(ptr),                               \
                             (long)(plasma_atomic_tmp + (addval)))), 0)); \
        return plasma_atomic_tmp;                                         \
    } while (0)

  #define plasma_atomic_fetch_add_u32_implreturn(ptr, addval)             \
    do {                                                                  \
        register uint32_t plasma_atomic_tmp;                              \
        do { plasma_atomic_tmp = (uint32_t)__lwarx((int *)(ptr))          \
        } while (__builtin_expect(                                        \
                   !(__stdcx((int *)(ptr),                                \
                             (int)(plasma_atomic_tmp + (addval)))), 0));  \
        return plasma_atomic_tmp;                                         \
    } while (0)

#endif

#ifndef plasma_atomic_fetch_add_ptr_impl
  /*(64-bit Windows is LLP64, not LP64 ABI, so different macro for 64-bit ptr)*/
  #if defined(_LP64) || defined(__LP64__) || defined(_WIN64)  /* 64-bit */
    #ifdef  plasma_atomic_fetch_add_u64_impl
    #define plasma_atomic_fetch_add_ptr_impl(ptr,addval) \
            plasma_atomic_fetch_add_u64_impl((ptr),(addval))
    #endif
  #else
    #ifdef  plasma_atomic_fetch_add_u32_impl
    #define plasma_atomic_fetch_add_ptr_impl(ptr,addval) \
            plasma_atomic_fetch_add_u32_impl((ptr),(addval))
    #endif
  #endif
#endif

/* FUTURE: might make into inline function for consistent return type casting
 * and to avoid potential multiple evaluation of macro arguments */
#ifndef plasma_atomic_fetch_add_u64_implreturn

  #define plasma_atomic_fetch_add_ptr(ptr,addval) \
          plasma_atomic_fetch_add_ptr_impl((ptr),(addval))
  #define plasma_atomic_fetch_add_u64(ptr,addval) \
          plasma_atomic_fetch_add_u64_impl((ptr),(addval))
  #define plasma_atomic_fetch_add_u32(ptr,addval) \
          plasma_atomic_fetch_add_u32_impl((ptr),(addval))

#else  /* defined(plasma_atomic_fetch_add_u64_implreturn) */

__attribute_regparm__((2))
void *  C99INLINE
plasma_atomic_fetch_add_ptr (void * const ptr, ptrdiff_t addval)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
__attribute_regparm__((2))
void *  C99INLINE
plasma_atomic_fetch_add_ptr (void * const ptr, ptrdiff_t addval)
{
  #if defined(_LP64) || defined(__LP64__)
    plasma_atomic_fetch_add_u64_implreturn(ptr, addval);
  #else
    plasma_atomic_fetch_add_u32_implreturn(ptr, addval);
  #endif
}
#endif

__attribute_regparm__((2))
uint64_t  C99INLINE
plasma_atomic_fetch_add_u64 (uint64_t * const ptr, uint64_t addval)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
__attribute_regparm__((2))
uint64_t  C99INLINE
plasma_atomic_fetch_add_u64 (uint64_t * const ptr, uint64_t addval)
{
    plasma_atomic_fetch_add_u64_implreturn(ptr, addval);
}
#endif

__attribute_regparm__((2))
uint32_t  C99INLINE
plasma_atomic_fetch_add_u32 (uint32_t * const ptr, uint32_t addval)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
__attribute_regparm__((2))
uint32_t  C99INLINE
plasma_atomic_fetch_add_u32 (uint32_t * const ptr, uint32_t addval)
{
    plasma_atomic_fetch_add_u32_implreturn(ptr, addval);
}
#endif

#endif /* defined(plasma_atomic_fetch_add_u64_implreturn) */


#ifdef __cplusplus
}
#endif


#endif

/*
 * References
 *
 * GCC
 * http://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html
 * http://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
 *
 * MSVC
 * http://msdn.microsoft.com/en-us/library/ms686360%28v=vs.85%29.aspx
 * http://msdn.microsoft.com/en-us/library/ttk2z1ws%28v=vs.90%29.aspx
 * http://msdn.microsoft.com/en-us/library/1s26w950%28v=vs.90%29.aspx
 * http://msdn.microsoft.com/en-us/library/1b4s3xf5%28v=vs.90%29.aspx
 * http://msdn.microsoft.com/en-us/library/853x471w%28v=VS.90%29.aspx
 * http://msdn.microsoft.com/en-us/library/191ca0sk%28v=vs.90%29.aspx
 *
 * OSX
 * OSAtomic.h Reference
 * http://developer.apple.com/library/mac/#documentation/System/Reference/OSAtomic_header_reference/Reference/reference.html#//apple_ref/doc/uid/TP40011482
 *
 * Solaris
 * atomic_cas (3C)
 * http://docs.oracle.com/cd/E23824_01/html/821-1465/atomic-cas-3c.html
 * atomic_swap (3C)
 * http://docs.oracle.com/cd/E23824_01/html/821-1465/atomic-swap-3c.html
 * atomic_add (3C)
 * http://docs.oracle.com/cd/E19253-01/816-5168/6mbb3hr0s/index.html
 *
 * AIX on POWER and xlC
 * Synchronization and atomic built-in functions
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/index.jsp?topic=%2Fcom.ibm.xlcpp121.aix.doc%2Fcompiler_ref%2Fbifs_sync_atomic.html
 * __compare_and_swap, __compare_and_swaplp
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/index.jsp?topic=%2Fcom.ibm.xlcpp121.aix.doc%2Fcompiler_ref%2Fbif_compare_and_swap_compare_and_swaplp.html
 * http://pic.dhe.ibm.com/infocenter/aix/v7r1/index.jsp?topic=%2Fcom.ibm.aix.basetechref%2Fdoc%2Fbasetrf1%2Fcompare_and_swap.htm
 * http://www.ibm.com/developerworks/systems/articles/powerpc.html
 * xlC v12.1 adds gcc-compatible intrinsics
 * (xlC might add full memory barriers for __sync_bool_compare_and_swap())
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/index.jsp?topic=%2Fcom.ibm.xlcpp121.aix.doc%2Fcompiler_ref%2Fbif_gcc_atomic_bool_comp_swap.html
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/index.jsp?topic=%2Fcom.ibm.xlcpp121.aix.doc%2Fcompiler_ref%2Fbifs_sync_atomic.html
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/index.jsp?topic=%2Fcom.ibm.xlc121.aix.doc%2Fcompiler_ref%2Fbif_fetch_and_swap_fetch_and_swaplp.html
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/topic/com.ibm.xlcpp121.aix.doc/compiler_ref/bifs_sync_load.html
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/topic/com.ibm.xlcpp121.aix.doc/compiler_ref/bifs_sync_store.html
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/index.jsp?topic=%2Fcom.ibm.xlc121.aix.doc%2Fcompiler_ref%2Fbif_gcc_atomic_fetch_add.html
 *
 * HP-UX on Itanium
 * Implementing Spinlocks on the Intel (R) Itanium (R) Architecture and PA-RISC
 * http://h21007.www2.hp.com/portal/download/files/unprot/itanium/spinlocks.pdf
 * inline assembly for Itanium-based HP-UX
 * http://h21007.www2.hp.com/portal/download/files/unprot/Itanium/inline_assem_ERS.pdf
 *
 *
 * xchg vs cmpxchg
 * https://lkml.org/lkml/2010/12/18/59
 * Implementing Scalable Atomic Locks for Multi-Core Intel(R) EM64T and IA32 Architectures
 * http://software.intel.com/en-us/articles/implementing-scalable-atomic-locks-for-multi-core-intel-em64t-and-ia32-architectures/
 *
 * http://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor
 */

/* NOTE: *not* handled in plasma_atomic.h: AMD Opteron Rev E hardware bug
 *
 * http://bugs.mysql.com/bug.php?id=26081
 * http://blogs.utexas.edu/jdm4372/2011/04/02/amd-opteron-processor-models-families-and-revisions/
 * http://en.wikipedia.org/wiki/List_of_AMD_Opteron_microprocessors
 * https://bugzilla.kernel.org/show_bug.cgi?id=11305
 * https://patchwork.kernel.org/patch/19429/
 * http://freebsd.1045724.n5.nabble.com/PATCH-AMD-Opteron-Rev-E-hack-td3929691.html
 *
 * AMD Opteron Rev E hardware bug
 * - First Generation Opteron: models 1xx, 2xx, 8xx.
 * - only Rev E (family 15, models 32-63), and some pre-release Rev F
 * - i.e. some first generation Opteron chips, released 2006 and earlier
 * - only exhibited on multi-core chips, or in multi-CPU systems
 * - second generation Opterons (not affected by bug) released in mid-2006
 *
 * NOTE: *not* handled in plasma_atomic.h; systems affected are 5+ years old
 *
 * http://support.amd.com/us/Processor_TechDocs/25759.pdf
 * Errata #147
 *
 * 147 Potential Violation of Read Ordering Rules Between Semaphore Operations
 *     and Unlocked Read-Modify-Write Instructions
 *
 * Description
 * Under a highly specific set of internal timing circumstances, the memory read ordering between a semaphore operation and a subsequent read-modify-write instruction (an instruction that uses the same memory location as both a source and destination) may be incorrect and allow the read-modify-write instruction to operate on the memory location ahead of the completion of the semaphore operation. The erratum will not occur if there is a LOCK prefix on the read-modify-write instruction.
 * This erratum does not apply if the read-only value in MSRC001_1023h[33] is 1b
 *
 * Potential Effect on System
 * In the unlikely event that the condition described above occurs, the read-modify-write instruction (in the critical section) may operate on data that existed prior to the semaphore operation. This erratum can only occur in multiprocessor or multicore configurations.
 *
 * Suggested Workaround
 * To provide a workaround for this unlikely event, software can perform any of the following actions for multiprocessor or multicore systems:
 *   - Place a LFENCE instruction between the semaphore operation and any subsequent read-modify-write instruction(s) in the critical section.
 *   - Use a LOCK prefix with the read-modify-write instruction.
 *   - Decompose the read-modify-write instruction into separate instructions.
 * No workaround is necessary if software checks that MSRC001_1023h[33] is set on all processors that may execute the code. The value in MSRC001_1023h[33] may not be the same on all processors in a multi-processor system.
 */

/*
 * plasma_atomic.h aims for simplicity and support of a small set of modern
 * hardware and compilers.  Authoritative sources provided above.
 *
 * There are many other atomics implementations in open source software,
 * available under a variety of open source licenses.  Here are some:
 *
 * BSD-style
 *
 * http://www.postgresql.org/
 * https://developers.google.com/native-client/
 * https://code.google.com/p/gperftools/
 * http://svnweb.freebsd.org/base/head/include/stdatomic.h?view=markup
 * http://svn.freebsd.org/base/projects/amd64_xen_pv/sys/i386/include/atomic.h
 * http://www.concurrencykit.org/
 * git://git.concurrencykit.org/ck.git
 * https://bitbucket.org/scitecwri/atomic_x86_ops/
 * http://git.chaoticmind.net/cgi-bin/cgit.cgi/boost.atomic/tree/boost/atomic
 *
 * GPL
 *
 * https://www.kernel.org/
 * http://gcc.gnu.org/
 * http://open-vm-tools.sourceforge.net/
 * git://git.opensource.vmware.com/opensource/open-vm-tools  vm_atomic.h
 * https://github.com/ivmai/libatomic_ops
 * http://qt-project.org/
 * http://qt.gitorious.org/qt/qt
 */