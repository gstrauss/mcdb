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

#include "plasma_attr.h"
#include "plasma_membar.h"

/* plasma_atomic_CAS_ptr - atomic pointer compare-and-swap (CAS)
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
 * FUTURE: Current plasma_atomic implementation done with compiler intrinsics.
 *         Intrinsics might provide stronger (slower) memory barriers than
 *         required.  Might provide assembly code implementations with more
 *         precise barriers and to support additional compilers on each platform
 */

#if defined(_MSC_VER)

  #include <intrin.h>
  #pragma intrinsic(_InterlockedCompareExchange)
  #pragma intrinsic(_InterlockedCompareExchange64)

  #define plasma_atomic_CAS_64_impl(ptr, cmpval, newval) \
          (_InterlockedCompareExchange64((ptr),(newval),(cmpval)) == (cmpval))
  #define plasma_atomic_CAS_32_impl(ptr, cmpval, newval) \
          (_InterlockedCompareExchange((ptr),(newval),(cmpval)) == (cmpval))

  /* NB: caller needs to include proper header for InterlockedCompareExchange*()
   *     (e.g. #include <windows.h>) */
  /* do not set these in header; calling file should determine need
   * http://support.microsoft.com/kb/166474
  #define VC_EXTRALEAN
  #define WIN32_LEAN_AND_MEAN
  #define plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval) \
          (InterlockedCompareExchangePointerNoFence((ptr),(newval), \
                                                          (cmpval)) == (cmpval))
  #define plasma_atomic_CAS_64_impl(ptr, cmpval, newval) \
          (InterlockedCompareExchange64NoFence((ptr),(newval), \
                                                     (cmpval)) == (cmpval))
  #define plasma_atomic_CAS_32_impl(ptr, cmpval, newval) \
          (InterlockedCompareExchangeNoFence((ptr),(newval), \
                                                   (cmpval)) == (cmpval))
   */

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

  #error "compare-and-swap not implemented for platform+compiler; suggestions?"

#endif

/* default plasma_atomic_CAS_ptr_impl() (if not previously defined) */
#ifndef plasma_atomic_CAS_ptr_impl
  #if defined(_LP64) || defined(__LP64__)  /* 64-bit */
    #define plasma_atomic_CAS_ptr_impl(ptr,  cmpval,  newval) \
            plasma_atomic_CAS_64_impl((uint64_t *)(ptr), \
                                      (uint64_t)(cmpval),(uint64_t)(newval))
  #else
    #define plasma_atomic_CAS_ptr_impl(ptr,  cmpval,  newval) \
            plasma_atomic_CAS_32_impl((uint32_t *)(ptr), \
                                      (uint32_t)(cmpval),(uint32_t)(newval))
  #endif
#endif


/* basic lock (32-bit) (0 == unlocked, 1 == locked)
 * provides acquire semantics on lock, release semantics on unlock
 * NB: unlike a mutex, plasma_atomic_lock_acquire() provides acquire semantics
 * and -not- sequential consistency (which would include StoreLoad barrier) */

/* default plasma_atomic_lock_* implementation */
#define PLASMA_ATOMIC_LOCK_INITIALIZER 0
#define plasma_atomic_lock_init(ptr) (*(ptr) = 0)
#ifndef plasma_atomic_lock_acquire
#define plasma_atomic_lock_acquire(ptr) \
        (plasma_atomic_CAS_32(ptr, 0, 1) \
          && (plasma_membar_atomic_thread_fence_acq_rel(), true))
#endif
#ifndef plasma_atomic_lock_release
#define plasma_atomic_lock_release(ptr) \
        (plasma_membar_atomic_thread_fence_release(), *(ptr) = 0)
#endif

/* (some platforms might have better mechanism for basic 32-bit lock) */
#if defined(__x86_64__) || defined(__i386__)
  /* CAS is implemented on x86 with LOCK prefix on xchg or cmpxchg instructions
   * (results in full memory barrier, so do not repeat barrier (see above)) */
  #undef  plasma_atomic_lock_acquire
  #define plasma_atomic_lock_acquire(ptr) \
          plasma_atomic_CAS_32_impl((ptr), 0, 1)
#elif defined(__sparc) || defined(__sparc__)
  /* TSO (total store order) on SPARC, so do not repeat barrier (see above)) */
  #undef  plasma_atomic_lock_acquire
  #define plasma_atomic_lock_acquire(ptr) \
          plasma_atomic_CAS_32_impl((ptr), 0, 1)
#elif defined(__ia64__)
  /* Itanium can extend ld and st instructions with .acq and .rel modifiers
   * Itanium volatile variables have implicit ld.acq and st.rel semantics
   * (so avoid separate barrier added above in default lock implementation) */
  #undef  plasma_atomic_lock_release
  #define plasma_atomic_lock_release(ptr) \
          (plasma_membar_ccfence(), *((volatile int * restrict)(ptr)) = 0)
  #if (defined(__HP_cc__) || defined(__HP_aCC__))
    #undef  plasma_atomic_lock_acquire
    #undef  plasma_atomic_lock_release
    #define plasma_atomic_lock_acquire(ptr) \
            !_Asm_xchg(_SZ_W, (ptr), 1, _LDHINT_NONE)
            /* 'xchg' instruction has 'acquire' semantics on Itanium */
    #define plasma_atomic_lock_release(ptr) \
            _Asm_st_volatile(_SZ_W, _STHINT_NONE, (ptr), 0)
            /* _Asm_st_volatile has 'release' semantics on Itanium
             * even if code compiled with +Ovolatile=__unordered */
  #elif defined (_MSC_VER)
    #pragma intrinsic(_InterlockedCompareExchange_acq)
    #undef  plasma_atomic_lock_acquire
    #define plasma_atomic_lock_acquire(ptr, cmpval, newval) \
            (_InterlockedCompareExchange_acq((ptr),(newval),(cmpval))==(cmpval))
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
 *   (might be required for some intrinsics) */

bool  C99INLINE
plasma_atomic_CAS_ptr (void ** const ptr, void *cmpval, void * const newval)
  __attribute_nonnull_x__((1));
#if !defined(NO_C99INLINE)
bool  C99INLINE
plasma_atomic_CAS_ptr (void ** const ptr, void *cmpval, void * const newval)
{
    return plasma_atomic_CAS_ptr_impl(ptr, cmpval, newval);
}
#endif

bool  C99INLINE
plasma_atomic_CAS_64 (uint64_t * const ptr,
                      uint64_t cmpval, const uint64_t newval)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
bool  C99INLINE
plasma_atomic_CAS_64 (uint64_t * const ptr,
                      uint64_t cmpval, const uint64_t newval)
{
    return plasma_atomic_CAS_64_impl(ptr, cmpval, newval);
}
#endif

bool  C99INLINE
plasma_atomic_CAS_32 (uint32_t * const ptr,
                      uint32_t cmpval, const uint32_t newval)
  __attribute_nonnull__;
#if !defined(NO_C99INLINE)
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
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms686360%28v=vs.85%29.aspx#interlocked_functions
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ttk2z1ws%28v=vs.85%29.aspx
 *
 * OSX
 * OSAtomic.h Reference
 * http://developer.apple.com/library/mac/#documentation/System/Reference/OSAtomic_header_reference/Reference/reference.html#//apple_ref/doc/uid/TP40011482
 *
 * Solaris
 * atomic_cas (3C)
 * http://docs.oracle.com/cd/E23824_01/html/821-1465/atomic-cas-3c.html
 *
 * AIX on POWER and xlC
 * Synchronization and atomic built-in functions
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/index.jsp?topic=%2Fcom.ibm.xlcpp121.aix.doc%2Fcompiler_ref%2Fbifs_sync_atomic.html
 * __compare_and_swap, __compare_and_swaplp
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/index.jsp?topic=%2Fcom.ibm.xlcpp121.aix.doc%2Fcompiler_ref%2Fbif_compare_and_swap_compare_and_swaplp.html
 * http://pic.dhe.ibm.com/infocenter/aix/v7r1/index.jsp?topic=%2Fcom.ibm.aix.basetechref%2Fdoc%2Fbasetrf1%2Fcompare_and_swap.htm
 * http://www.ibm.com/developerworks/systems/articles/powerpc.html
 * xlC v12.1 adds gcc-compatible intrinsic, but also adds full memory barrier
 * http://pic.dhe.ibm.com/infocenter/comphelp/v121v141/index.jsp?topic=%2Fcom.ibm.xlcpp121.aix.doc%2Fcompiler_ref%2Fbif_gcc_atomic_bool_comp_swap.html
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
