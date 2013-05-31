/*
 * plasma_membar - portable macros for processor-specific memory barriers
 *
 *   inline assembly and compiler intrinsics for CPU memory barriers
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

/* Preface:
 *
 * The contents herein were created by carefully reading online documentation.
 * References are provided at the bottom of the file.
 *
 * This code is not nearly as well-tested as road-hardened open source
 * products (e.g. Linux kernel, Postgres, ...).  Still, this code is
 * intended as a portable set of some of the operations used in high
 * performance multi-processor-aware algorithms.
 *
 * Motivation:
 *
 * So why another implementation of memory barriers?
 * mcdb needed portable consume and StoreStore barriers and so a few small
 * assembly tools, not an unabridged toolbox.  Collecting this assembly was
 * approached as a research project and learning experience.
 *
 * C11 and C++11 atomics aim for higher level interfaces, are not yet supported
 * by all compilers, and even when supported, the conforming version might not
 * be available on a given system on which mcdb is run.  (Once widely available,
 * atomics with integrated ordering constraints should be preferred over the
 * independent fences (below) for the benefit of compiler optimization.)
 *
 * The barriers below are (some) of the building blocks for higher level
 * atomics, and can be combined with inline assembly instructions
 * (not included here) which provide bus locking and exclusive access
 * to cache memory.  One of the more expensive ways to implement barriers
 * is by using pthread_mutex_lock() and pthread_mutex_unlock() around
 * memory access.  pthread_mutex_lock() is a potentially blocking operation.
 * The barriers below, when used properly, aim to do be nonblocking and faster,
 * yet still enable correct behavior.
 *
 * mcdb aims to use these barriers in fast code paths to cooperate with its use
 * of pthread_mutex_lock() in less frequent or already more expensive code paths
 * (e.g. those that make system calls).
 */

/*
 * memory barriers
 *
 * The below macros might be insufficient memory barriers if not used correctly!
 *
 * Documentation of proper use of memory barriers *is not* provided here,
 * though an attempt is made to reference authoritative documentation, or at
 * least primary sources, for each processor and compiler that is listed here.
 *
 * Proper memory semantics on multi-CPU and multi-core systems are complex
 * and vary depending on memory model implemented by an architecture, CPU
 * generation, and compilers.  A barrier needs to be considered not only for
 * how it affects the processor on which the barrier instruction(s) is run,
 * but also by how the barrier affects visibility to other processors
 * (and for other processors, the current processor is "other").  
 *
 * Memory barriers are not always explicit CPU instructions.  They can be one
 * or more CPU instructions that result in implementing the desired barrier,
 * and sometimes different (sets of) instructions can result in same barrier.
 * Modern CPUs have many levels of caching, out-of-order issue, and speculation.
 * Proper implementation of memory barriers must account for these and other
 * features of the CPU architecture.
 *
 * plasma_membar.h defines the following macros for memory barriers:
 *
 * plasma_membar_ccfence()     compiler fence (cc optimization/reorder fence)
 * plasma_membar_LoadLoad()    load  instr ordering with respect to prior loads
 * plasma_membar_StoreStore()  store flush ordering with respect to prior stores
 * plasma_membar_ld_consumer() (equivalent to plasma_membar_LoadLoad())
 * plasma_membar_st_producer() (equivalent to plasma_membar_StoreStore())
 * plasma_membar_ld_ctrldep()  ld/st instr ordering with respect to prior loads
 * plasma_membar_ld_datadep()  C11 'consume' load,barrier,load w/data dependency
 * plasma_membar_ld_acq()      C11 'acquire' load,barrier
 * plasma_membar_st_rel()      C11 'release' barrier,store
 * plasma_membar_rmw_acq_rel() C11 'acquire-release' read-modify-write,barrier
 * plasma_membar_seq_cst()     C11 'sequential consistency' barrier
 *
 * atomic_thread_fence(...)    C11 generic mem order synchronization primitive
 * atomic_signal_fence(...)    C11 compiler fence
 * C11 atomic_thread_fence() and atomic_signal_fence() are defined in terms of
 * plasma_membar_* macros if C11 atomic definitions are not detected.
 *
 * Some compilers might not properly inline the assembly if optimization is
 * disabled (e.g. Sun Studio and CAS instructions).  Some instructions, such
 * as __isync on POWER, place additional requirements on how the instructions
 * must be used for effectiveness, e.g. after a load-compare-branch sequence.
 * On x86 (ix86, x86_64), fast-string operations should be followed by a store
 * release memory barrier (sfence) and a store to a native-sized type for
 * proper memory ordering on different processor which checks the native-sized
 * type and issues an acquire memory barrier before accessing the results of
 * the fast-string operation.  (fast-string operations are special assembly ops,
 * so you should know if you are using them)
 *
 * Memory barriers below are for multi-CPU/multi-core/SMT processors and might
 * be overly specified (could be simpler or more relaxed) for non-SMP systems.
 * These memory barriers are aimed at use with general cacheable memory regions.
 *
 * DMA (direct memory access) and MMIO (memory-mapped I/O) to devices might
 * require additional or different barriers than those provided here; the
 * macros below are not aimed at supporting such device access.  For example,
 * IBM POWER __eieio() is not used below, but required for some memory ordering
 * pertaining to device access.  Similarly, membar #MemIssue is needed on SPARC
 * for proper access to MMIO.  The barriers in the macros below are not intended
 * for and most are not strong enough for use in writing device drivers.
 *
 *
 * Additional documentation and references can be found at bottom of this file.
 *
 */

#ifndef INCLUDED_PLASMA_MEMBAR_H
#define INCLUDED_PLASMA_MEMBAR_H


#if defined(__x86_64__) || defined(__i386__)
/* Intel(R) 64 and IA-32 Architectures Developer's Manual: Vol. 3A
 * SSE 3DNOW clflush and non-temporal move instructions require stronger
 * barriers than simple compiler fence, but compiler fence handles common case
 * AMD64 Architecture Programmer's Manual Volume 2: System Programming
 *   Table 7-3. Memory Access Ordering Rules
 * provides an excellent reference table
 * Implementation note:
 * "":::"memory" is an empty asm instruction that tells
 * gcc that the statement might modify any part of memory, and therefore the
 * compiler must not keep any non-local values in registers across the statement
 *   asm as "memory barrier"
 *     http://gcc.gnu.org/ml/gcc/2003-04/msg01180.html
 * (http://netbsd.2816.n7.nabble.com/Why-does-membar-consumer-do-anything-on-x86-64-td193551.html)
 * Implementation note:
 * All 'lock' atomic instructions on x86 provide implicit 'mfence' semantics,
 * at least for typical memory access instructions (see note above about SSE).
 * Since 'lock; addl' can be faster than 'mfence' on AMD, and since 'lock; addl'
 * is supported on all x86 chips (even older chips), use 'lock; addl' even when
 * 'mfence' is available ( #if defined(__SSE2__) || defined(__x86_64__) ).
 *   volatile fences should prefer lock:addl to actual mfence instructions
 *     http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=6822204
 *   http://openjdk.5641.n7.nabble.com/MFENCE-vs-LOCK-addl-td11517.html
 *   http://stackoverflow.com/questions/4232660/which-is-a-better-write-barrier-on-x86-lockaddl-or-xchgl
 */
/*#ifdef __x86_64__*/
/*#define plasma_membar_LoadLoad()   __asm__ __volatile__("lfence":::"memory")*/
/*#define plasma_membar_StoreStore() __asm__ __volatile__("sfence":::"memory")*/
/*#define plasma_membar_st_rel()     __asm__ __volatile__("mfence":::"memory")*/
/*#define plasma_membar_ld_acq()     __asm__ __volatile__("mfence":::"memory")*/
/*#define plasma_membar_seq_cst()    __asm__ __volatile__("mfence":::"memory")*/
/*#endif*/
#define plasma_membar_ccfence()     __asm__ __volatile__ ("":::"memory")
#define plasma_membar_LoadLoad()    __asm__ __volatile__ ("":::"memory")
#define plasma_membar_StoreStore()  __asm__ __volatile__ ("":::"memory")
#define plasma_membar_ld_ctrldep()  __asm__ __volatile__ ("":::"memory")
#define plasma_membar_ld_datadep()  do { } while (0)
#define plasma_membar_ld_acq()      __asm__ __volatile__ ("":::"memory")
#define plasma_membar_st_rel()      __asm__ __volatile__ ("":::"memory")
#define plasma_membar_rmw_acq_rel() __asm__ __volatile__ ("":::"memory")
#ifdef __x86_64__
#define plasma_membar_seq_cst()     __asm__ __volatile__ ("lock; addl $0,0(%%rsp)":::"memory")
#else /* __i386__ */
#define plasma_membar_seq_cst()     __asm__ __volatile__ ("lock; addl $0,0(%%esp)":::"memory")
#endif
#endif


#if defined(__ia64) || defined(__ia64__)
/* IA64 instruction set contains ld.acq and st.rel variants that are finer
 * than full memory barriers, but not available as standalone barriers.
 * C11 atomics (not here) provide interfaces to generate these instructions.
 * Atomics (cmpxchg, xchg, fetchadd) on IA64 provide implicit acq/rel memory
 * barriers (related by http://g.oswego.edu/dl/jmm/cookbook.html), and therefore
 * plasma_membar_rmw_acq_release() might be made same as ccfence (XXX: TBD).
 */
#define plasma_membar_ccfence()     __asm__ __volatile__ ("":::"memory")
#define plasma_membar_LoadLoad()    __asm__ __volatile__ ("mf":::"memory")
#define plasma_membar_StoreStore()  __asm__ __volatile__ ("mf":::"memory")
#define plasma_membar_ld_ctrldep()  __asm__ __volatile__ ("mf":::"memory")
#define plasma_membar_ld_datadep()  do { } while (0)
#define plasma_membar_ld_acq()      __asm__ __volatile__ ("mf":::"memory")
#define plasma_membar_st_rel()      __asm__ __volatile__ ("mf":::"memory")
#define plasma_membar_rmw_acq_rel() __asm__ __volatile__ ("mf":::"memory")
#define plasma_membar_seq_cst()     __asm__ __volatile__ ("mf":::"memory")
#endif


#ifdef __arm__
/* ARM and Thumb Instructions > Miscellaneous Instructions > DMB, DSB, and ISB
 * http://infocenter.arm.com/help/topic/com.arm.doc.dui0489h/CIHGHHIE.html
 *   dmb (Data Memory Barrier)
 *   dsb (Data Synchronization Barrier)
 *   isb (Instruction Synchronization Barrier)
 * ARMv8 Instruction Set Overview
 * http://infocenter.arm.com/help/topic/com.arm.doc.genc010197a/index.html
 * ("oshld" in ARMv8+; change to just "osh" for ARMv7)
 * XXX: TBD: might LoadLoad be satisfied with dmb ishld instead of dmb oshld?
 *      (plasma_ld_ctrldep() is still better when it can be used) */
/*(defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM-0 >= 8)*/
#ifdef __CC_ARM
#define plasma_membar_ccfence()     __schedule_barrier()
#else
#define plasma_membar_ccfence()     __asm__ __volatile__("":::"memory")
#endif
/*#define plasma_membar_LoadLoad()    __asm__ __volatile__("dmb oshld":::"memory")*/
#define plasma_membar_LoadLoad()    __asm__ __volatile__("dmb osh":::"memory")
#define plasma_membar_StoreStore()  __asm__ __volatile__("dmb oshst":::"memory")
#define plasma_membar_ld_ctrldep()  __asm__ __volatile__("isb":::"memory")
#define plasma_membar_ld_datadep()  do { } while (0)
#define plasma_membar_ld_acq()      __asm__ __volatile__("dmb osh":::"memory")
#define plasma_membar_st_rel()      __asm__ __volatile__("dmb osh":::"memory")
#define plasma_membar_rmw_acq_rel() __asm__ __volatile__("dmb osh":::"memory")
#define plasma_membar_seq_cst()     __asm__ __volatile__("dmb osh":::"memory")
/* For MMIO, data cache clean DCC* or data cache invalidate DCI* family of
 * functions are needed along with DMB.  DSB is needed instead of DMB when
 * used in conjunction with interrupts or events such as WFI or WFE */
#endif


#if defined(__sparc) || defined(__sparc__)
/* https://github.com/joyent/illumos-joyent/blob/master/usr/src/common/atomic/sparcv9/atomic.s */
/* membar #StoreLoad by itself has same effect on SPARC as all of 
 * #StoreLoad|#StoreStore|#LoadLoad|#LoadStore in plasma_membar_seq_cst() */
#define plasma_membar_ccfence()     __asm __volatile__ ("":::"memory")
#define plasma_membar_LoadLoad()    __asm __volatile__ ("membar #LoadLoad":::"memory")
#define plasma_membar_StoreStore()  __asm __volatile__ ("membar #StoreStore":::"memory")
#define plasma_membar_ld_ctrldep()  __asm __volatile__ ("membar #LoadStore|#LoadLoad":::"memory")
#define plasma_membar_ld_datadep()  do { } while (0)
#define plasma_membar_ld_acq()      __asm __volatile__ ("membar #StoreLoad|#StoreStore":::"memory")
#define plasma_membar_st_rel()      __asm __volatile__ ("membar #LoadStore|#StoreStore":::"memory")
#define plasma_membar_rmw_acq_rel() __asm __volatile__ ("membar #LoadStore|#StoreStore|#LoadLoad":::"memory")
#define plasma_membar_seq_cst()     __asm __volatile__ ("membar #StoreLoad|#StoreStore|#LoadLoad|#LoadStore":::"memory")
/* membar #MemIssue or #Sync additionally needed in some barriers for MMIO */

/* Solaris on SPARC uses TSO (total store order)
 * http://dsc.sun.com/solaris/articles/atomic_sparc/
 * https://blogs.oracle.com/d/entry/compiler_memory_barriers */
/* Linux on SPARC uses TSO (total store order)
 * (not PSO (partial store order) or RMO (relaxed memory order)) */
#if (defined(__sun) && defined(__SVR4)) || defined(__linux__)
#undef  plasma_membar_LoadLoad
#undef  plasma_membar_StoreStore
#undef  plasma_membar_ld_ctrldep
#undef  plasma_membar_ld_acq
#undef  plasma_membar_st_rel
#undef  plasma_membar_rmw_acq_rel
#define plasma_membar_LoadLoad()    __asm __volatile__ ("":::"memory")
#define plasma_membar_StoreStore()  __asm __volatile__ ("":::"memory")
#define plasma_membar_ld_ctrldep()  __asm __volatile__ ("":::"memory")
#define plasma_membar_ld_acq()      __asm __volatile__ ("":::"memory")
#define plasma_membar_st_rel()      __asm __volatile__ ("":::"memory")
#define plasma_membar_rmw_acq_rel() __asm __volatile__ ("":::"memory")
#endif
#endif


#if defined(__ppc__)   || defined(_ARCH_PPC)  || \
    defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER)
#define plasma_membar_ccfence()     __asm__ __volatile__ ("":::"memory")
#define plasma_membar_LoadLoad()    __asm__ __volatile__ ("lwsync":::"memory")
#define plasma_membar_StoreStore()  __asm__ __volatile__ ("lwsync":::"memory")
#define plasma_membar_ld_ctrldep()  __asm__ __volatile__ ("lwsync":::"memory")
#define plasma_membar_ld_datadep()  do { } while (0)
#define plasma_membar_ld_acq()      __asm__ __volatile__ ("lwsync":::"memory")
#define plasma_membar_st_rel()      __asm__ __volatile__ ("lwsync":::"memory")
#define plasma_membar_rmw_acq_rel() __asm__ __volatile__ ("lwsync":::"memory")
#define plasma_membar_seq_cst()     __asm__ __volatile__ ("isync \n\t lwsync":::"memory")
/*#define plasma_membar_seq_cst()     __asm__ __volatile__ ("sync":::"memory")*/
/* "isync \n\t lwsync" can substitute for "sync" where MMIO is not involved
 * and memory access is to typical cacheable memory.
 * http://lxr.evanmiller.org/http/source/os/unix/ngx_gcc_atomic_ppc.h?v=nginx-1.1.12 */
/* plasma_membar_ld_ctrldep() could be isync, but lwsync is generally faster on
 * modern POWER processors (see Additional Notes at end of file) */
/* (ppc assembler treats ';' as comment; use "\n\t" to separate assembly insn)*/
/* (could use isync, eieio on older G3/G4 PPC which do not support lwsync) */
/* "eieio" or "sync" needed as some barriers for MMIO */
#endif



/* prefer compiler intrinsics to inline assembly for fine-grained intrinsics
 * (and note plasma_membar_ld_datadep() does not require special instructions on
 *  most architectures and so is not redefined below for such architectures) */


/* GCC
 * http://gcc.gnu.org/onlinedocs/gcc/
 * http://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html
 * http://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
 * http://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
 * http://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
 *
 * clang
 * http://llvm.org/docs/LangRef.html
 * http://llvm.org/docs/Atomics.html
 * http://clang.llvm.org/docs/LanguageExtensions.html
 * http://libcxx.llvm.org/atomic_design.html
 * http://clang-developers.42468.n3.nabble.com/atomic-intrinsics-td1618127.html
 */
#if 0 /* inline assembly might be more fine-grained on certain architectures */
#if defined(__GNUC__) || defined(__clang__)
#undef  plasma_membar_seq_cst
#define plasma_membar_seq_cst()     __sync_synchronize()
/* ... redefine other macros depending on architecture ... */
#endif
#endif


/* Mac OS X (Apple and Mach kernel)
 * https://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man3/atomic.3.html
 * https://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man3/barrier.3.html
 * https://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man3/spinlock.3.html
 */
#if defined(__APPLE__) && defined(__MACH__)
#include <libkern/OSAtomic.h>
#undef  plasma_membar_seq_cst
#define plasma_membar_seq_cst()     OSMemoryBarrier()
/* use OSSynchronizeIO() for MMIO */
#endif


#ifdef __INTEL_COMPILER
#ifdef __ia64__
#include <ia64intrin.h>
#undef  plasma_membar_ccfence
#undef  plasma_membar_LoadLoad
#undef  plasma_membar_StoreStore
#undef  plasma_membar_ld_ctrldep
#undef  plasma_membar_ld_acq
#undef  plasma_membar_st_rel
#undef  plasma_membar_rmw_acq_rel
#undef  plasma_membar_seq_cst
#define plasma_membar_ccfence()     __memory_barrier()
#define plasma_membar_LoadLoad()    __mf()
#define plasma_membar_StoreStore()  __mf()
#define plasma_membar_ld_ctrldep()  __mf()
#define plasma_membar_ld_acq()      __mf()
#define plasma_membar_st_rel()      __mf()
#define plasma_membar_rmw_acq_rel() __mf()
#define plasma_membar_seq_cst()     __mf()
#endif
#ifdef __x86_64__
#undef  plasma_membar_ccfence
#undef  plasma_membar_seq_cst
#define plasma_membar_ccfence()     __memory_barrier()
#define plasma_membar_seq_cst()     _mm_mfence()
#endif
#endif


/* HP aCC (on HP-UX on Itanium) */
/* (using 'volatile' in HP aCC results, by default, in barriers on every access)
 * (aCC also provides additional volatile types with which to instrument code)
 * C11 atomics on Itanium generate more efficient load and store instructions
 * employing .acq and .rel attributes, respectively, on the load and store,
 * rather than these standalone (and more expensive) barriers */
#if defined(__HP_cc) || defined(__HP_aCC)
#if defined(__ia64) || defined(__ia64__)
#include <machine/sys/inline.h>
#undef  plasma_membar_ccfence
#undef  plasma_membar_LoadLoad
#undef  plasma_membar_StoreStore
#undef  plasma_membar_ld_ctrldep
#undef  plasma_membar_ld_acq
#undef  plasma_membar_st_rel
#undef  plasma_membar_rmw_acq_rel
#undef  plasma_membar_seq_cst
#define plasma_membar_ccfence()     _Asm_sched_fence(_UP_MEM_FENCE|_DOWN_MEM_FENCE)
#define plasma_membar_LoadLoad()    _Asm_mf(_UP_MEM_FENCE|_DOWN_MEM_FENCE)
#define plasma_membar_StoreStore()  _Asm_mf(_UP_MEM_FENCE|_DOWN_MEM_FENCE)
#define plasma_membar_ld_ctrldep()  _Asm_mf(_UP_MEM_FENCE|_DOWN_MEM_FENCE)
#define plasma_membar_ld_acq()      _Asm_mf(_UP_MEM_FENCE|_DOWN_MEM_FENCE)
#define plasma_membar_st_rel()      _Asm_mf(_UP_MEM_FENCE|_DOWN_MEM_FENCE)
#define plasma_membar_rmw_acq_rel() _Asm_mf(_UP_MEM_FENCE|_DOWN_MEM_FENCE)
#define plasma_membar_seq_cst()     _Asm_mf(_UP_MEM_FENCE|_DOWN_MEM_FENCE)
#endif
#endif


/* Oracle Solaris Studio 12.2+ */
/* Oracle Solaris Studio 12.2: C User's Guide 
 * 3.9 Memory Barrier Intrinsics
 * http://docs.oracle.com/cd/E18659_01/html/821-1384/gjzmf.html */
#if (defined(__SUNPRO_C)  && __SUNPRO_C  >= 0x5110) \
 || (defined(__SUNPRO_CC) && __SUNPRO_CC >= 0x5110)
#undef  plasma_membar_ccfence
#undef  plasma_membar_LoadLoad
#undef  plasma_membar_StoreStore
#undef  plasma_membar_ld_ctrldep
#undef  plasma_membar_ld_acq
#undef  plasma_membar_st_rel
#undef  plasma_membar_rmw_acq_rel
#undef  plasma_membar_seq_cst
#include <mbarrier.h>
#if 0 /* ideally results in just a compiler barrier on TSO SPARC and on x86 */
#define plasma_membar_LoadLoad()    __machine_r_barrier()
#define plasma_membar_StoreStore()  __machine_w_barrier()
#define plasma_membar_ld_ctrldep()  __machine_acq_barrier()
#define plasma_membar_ld_acq()      __machine_acq_barrier()
#define plasma_membar_st_rel()      __machine_rel_barrier()
#define plasma_membar_rmw_acq_rel() __machine_acq_barrier(), __machine_rel_barrier()
#endif
#define plasma_membar_ccfence()     __compiler_barrier()
#define plasma_membar_LoadLoad()    __compiler_barrier()
#define plasma_membar_StoreStore()  __compiler_barrier()
#define plasma_membar_ld_ctrldep()  __compiler_barrier()
#define plasma_membar_ld_acq()      __compiler_barrier()
#define plasma_membar_st_rel()      __compiler_barrier()
#define plasma_membar_rmw_acq_rel() __compiler_barrier()
#define plasma_membar_seq_cst()     __machine_rw_barrier()
#endif


/* IBM Visual Age xlC */
/* XL C for AIX, V11.1 Compiler Reference
 * http://pic.dhe.ibm.com/infocenter/comphelp/v111v131/index.jsp
 * Synchronization functions
 * http://pic.dhe.ibm.com/infocenter/comphelp/v111v131/index.jsp?topic=%2Fcom.ibm.xlc111.aix.doc%2Fcompiler_ref%2Fbifs_sync.html */
#if defined(__xlc__) || defined(__xlC__)  /*(__IBMC__, __IBMCPP__ for ver cmp)*/
#if defined(__ppc__)   || defined(_ARCH_PPC)  || \
    defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER)
#include <sys/atomic_op.h>
#undef  plasma_membar_ccfence
#undef  plasma_membar_LoadLoad
#undef  plasma_membar_StoreStore
#undef  plasma_membar_ld_ctrldep
#undef  plasma_membar_ld_acq
#undef  plasma_membar_st_rel
#undef  plasma_membar_rmw_acq_rel
#undef  plasma_membar_seq_cst
#define plasma_membar_ccfence()     __fence()
#define plasma_membar_LoadLoad()    __lwsync()
#define plasma_membar_StoreStore()  __lwsync()
#define plasma_membar_ld_ctrldep()  __lwsync()
#define plasma_membar_ld_acq()      __lwsync()
#define plasma_membar_st_rel()      __lwsync()
#define plasma_membar_rmw_acq_rel() __lwsync()
#define plasma_membar_seq_cst()     __isync(), __lwsync()
/*#define plasma_membar_seq_cst()     __sync()*/
/* "__eieio()" or "__sync()" needed as some barriers for MMIO */
#endif
#endif


/* MSVC compile /Oi (Generate Intrinsic Functions) to enable all intrinsics
 * http://technet.microsoft.com/en-us/subscriptions/26td21ds%28v=vs.100%29.aspx
 * http://msdn.microsoft.com/en-us/library/f99tchzc%28v=vs.100%29.aspx
 * http://msdn.microsoft.com/en-us/library/windows/desktop/f20w0x5e%28v=vs.85%29.aspx
 * MS Visual Studio 2008 improved compatibility between intrin.h, other hdrs */
#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_ReadWriteBarrier)
#pragma intrinsic(_ReadBarrier)
#pragma intrinsic(_WriteBarrier)
#if defined(_M_AMD64) || defined(_M_IX86)
#undef  plasma_membar_ccfence
#undef  plasma_membar_LoadLoad
#undef  plasma_membar_StoreStore
#undef  plasma_membar_ld_ctrldep
#undef  plasma_membar_ld_acq
#undef  plasma_membar_st_rel
#undef  plasma_membar_rmw_acq_rel
#undef  plasma_membar_seq_cst
#define plasma_membar_ccfence()     _ReadWriteBarrier()
#define plasma_membar_LoadLoad()    _ReadBarrier()
#define plasma_membar_StoreStore()  _WriteBarrier()
#define plasma_membar_ld_ctrldep()  _ReadWriteBarrier()
#define plasma_membar_ld_acq()      _ReadWriteBarrier()
#define plasma_membar_st_rel()      _ReadWriteBarrier()
#define plasma_membar_rmw_acq_rel() _ReadWriteBarrier()
/* prefer using intrinsics directly instead of winnt.h macro */
#if defined(_M_AMD64)
#pragma intrinsic(__faststorefence)
#define plasma_membar_seq_cst()     __faststorefence()
#else
#define plasma_membar_seq_cst()     MemoryBarrier()
#endif
/* NB: caller needs to include proper header for MemoryBarrier()
 *     (e.g. #include <windows.h>)
 * http://stackoverflow.com/questions/257134/weird-compile-error-dealing-with-winnt-h */
#if 0  /* do not set these in header; calling file should determine need */
/* http://support.microsoft.com/kb/166474 */
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <wtypes.h>
#include <windef.h>
#include <winnt.h>
#include <xmmintrin.h>
#endif
/* http://msdn.microsoft.com/en-us/library/ms684208.aspx (MemoryBarrier())
 * MemoryBarrier() can be faster than _mm_mfence() on some architectures,
 * (e.g. AMD) due to the serializing effects of locking the bus being sufficient
 * to provide the barrier.  See comments with __x86_64__ code above.
 * http://stackoverflow.com/questions/6480610/what-is-the-behavior-of-faststorefence 
 * http://stackoverflow.com/questions/12308916/mm-sfence-vs-faststorefence
 * http://msdn.microsoft.com/en-us/library/t710k390%28v=vs.90%29.aspx
 *
 * Overview of x64 Calling Conventions
 *   http://msdn.microsoft.com/en-us/library/ms235286.aspx */
#elif defined(_M_IA64) /* Microsoft no longer supports Windows on Itanium */
#pragma intrinsic(__mf)
#undef  plasma_membar_ccfence
#undef  plasma_membar_LoadLoad
#undef  plasma_membar_StoreStore
#undef  plasma_membar_ld_ctrldep
#undef  plasma_membar_ld_acq
#undef  plasma_membar_st_rel
#undef  plasma_membar_rmw_acq_rel
#undef  plasma_membar_seq_cst
#define plasma_membar_ccfence()     _ReadWriteBarrier()
#define plasma_membar_LoadLoad()    __mf()
#define plasma_membar_StoreStore()  __mf()
#define plasma_membar_ld_ctrldep()  __mf()
#define plasma_membar_ld_acq()      __mf()
#define plasma_membar_st_rel()      __mf()
#define plasma_membar_rmw_acq_rel() __mf()
#define plasma_membar_seq_cst()     __mf()
#else
#error "MSVC C does not support inline assembly and requires use of intrinsics"
/*(would need to add appropriate intrinsincs for any platform reaching here)*/
#endif
#endif



/* Some BSD derivatives (e.g. FreeBSD, Solaris) provide sys/atomic.h with their
 * own membar_* macros.  Care has been taken to avoid conflict w/ those macros.
 * Conceptual mapping:
 *   sys/atomic.h membar_consumer() -> plasma_membar_ld_consumer()
 *   sys/atomic.h membar_producer() -> plasma_membar_st_producer()
 *   sys/atomic.h membar_enter()    -> plasma_membar_ld_ctrldep() (or seq_cst())
 *   sys/atomic.h membar_exit()     -> plasma_membar_st_rel()
 *   sys/atomic.h membar_sync()     -> plasma_membar_seq_cst()
 * Note potential confusion over slightly different uses of the word 'consume'.
 * C11/C++11 'consume' is implemented by plasma_membar_ld_datadep() -- barrier
 * for use between loads where subsequent loads have data dependency on first
 * load -- and differs from BSD sys/atomic.h member_consumer() -- barrier for
 * use between loads and implemented by plasma_membar_ld_consumer(). */
#define plasma_membar_ld_consumer() plasma_membar_LoadLoad()
#define plasma_membar_st_producer() plasma_membar_StoreStore()




#if 0  /* INCOMPLETE plasma_membar_ifence() NOTES */


/* XXX: **must** verify instr fence on each platform; incomplete notes follow */


/* plasma_membar_dfence()    data hardware fence (== plasma_membar_seq_cst())
 * plasma_membar_ifence()    instruction hardware fence
 *
 * The hardware fences plasma_membar_dfence() and plasma_membar_ifence() are
 * equivalent on x86 platforms, though they differ on platforms where a
 * distinction is made between data and instruction streams.
 * plasma_membar_dfence() is intended for more typical use as a full barrier
 * sync for data.  plasma_membar_ifence() is intended for use as *part* of the
 * synchronization instructions required when the instruction stream has been
 * modified, e.g. with self-modifying code such as inside the Java compiler.
 * NOTE: ARM requires a full sequence of operations when updating instructions
 * and plasma_membar_ifence() DOES NOT perform all the required steps
 * (see additional documentation below).  NOTE: POWER requires a full sequence
 * of operations when updating instructions and plasma_membar_ifence() DOES NOT
 * perform all the required steps (see additional documentation below where
 * plasma_membar_ifence() is defined for POWER).  NOTE: SPARC requires a
 * 'flush' instruction when updating instructions.
 */


#define plasma_membar_dfence()      plasma_membar_seq_cst()


/* x86, x86_64, ia64 */
#if defined(__i386__) || defined(__x86_64__) \
 || defined(__ia64)   || defined(__ia64__)
#define plasma_membar_ifence()      plasma_membar_dfence()
#endif


#ifdef __arm__
/* NOTE: plasma_membar_ifence() is INSUFFICIENT on ARM!
 * Proper implementation here would require a new macro to which caller passed
 * a list of instructions (pointer to src, dest, and size of src) and the macro
 * took care of writing the instructions and flushing cache lines before issuing
 * the appropriate barriers.
 * http://infocenter.arm.com/help/topic/com.arm.doc.genc007826/index.html
 * 9.2.2 Ensuring the visibility of updates to instructions for a multiprocessor
 * documents a sequence of events required when updating instructions (e.g. for
 * self-modifying code) including data cache clean to point of unification,
 * data synchronization barrier, instruction cache line invalidate to point of
 * unification, branch predictor invalidate, data synchronization barrier, and
 * instruction synchronization barrier.
 */
#define plasma_membar_ifence()      __asm__ __volatile__ ("dsb sy; isb sy":::"memory")
#endif


#if defined(__sparc) || defined(__sparc__)
#define plasma_membar_ifence()      __asm __volatile__ ("flush %0":::"memory")
/* plasma_membar_ifence() above is incomplete;needs arg specifying %0 register*/
/* plasma_membar_ifence() might need membar #MemIssue or #Sync prior to flush */
#endif


#if defined(__ppc__)   || defined(_ARCH_PPC)  || \
    defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER)
/* NOTE: plasma_membar_ifence() is INSUFFICIENT on POWER!
 * Proper implementation here would require a new macro to which caller passed
 * a list of instructions (pointer to src, dest, and size of src) and the macro
 * took care of writing the instructions and flushing cache lines before issuing
 * the appropriate barriers.  See 1.8 Instruction Storage (page 663) in
 * PowerISA_V2.06B_V2_PUBLIC.pdf for dcbst X \n sync \n icbi X \n isync */
#define plasma_membar_ifence()      __asm__ __volatile__ ("sync":::"memory")
/* prefer compiler intrinsics to inline assembly for fine-grained intrinsics */
/* IBM Visual Age xlC */
#if defined(__xlc__) || defined(__xlC__)  /*(__IBMC__, __IBMCPP__ for ver cmp)*/
#include <sys/atomic_op.h>
#undef  plasma_membar_ifence
#define plasma_membar_ifence()      __sync()
/* AIX v7r1 provides _sync_cache_range() in libc.a to sync I cache with D cache
 * http://pic.dhe.ibm.com/infocenter/aix/v7r1/index.jsp?topic=%2Fcom.ibm.aix.libs%2F_.htm */
#endif
#endif


#endif  /* INCOMPLETE plasma_membar_ifence() NOTES */




/* C11 standard atomics
 *
 * limited implementation: atomic_thread_fence() and atomic_signal_fence() only
 * ~~~~~~~~~~~~~~~~~~~~~~~
 *
 * http://en.cppreference.com/w/c/atomic
 * http://en.cppreference.com/w/c/atomic/memory_order
 * http://en.cppreference.com/w/c/atomic/atomic_thread_fence
 * http://en.cppreference.com/w/c/atomic/atomic_signal_fence
 * http://en.cppreference.com/w/cpp/atomic
 * http://en.cppreference.com/w/cpp/atomic/memory_order
 * http://en.cppreference.com/w/cpp/atomic/atomic_thread_fence
 * http://en.cppreference.com/w/cpp/atomic/atomic_signal_fence
 */

#define plasma_membar_atomic_thread_fence_consume() plasma_membar_ld_datadep()
#define plasma_membar_atomic_thread_fence_acquire() plasma_membar_ld_acq()
#define plasma_membar_atomic_thread_fence_release() plasma_membar_st_rel()
#define plasma_membar_atomic_thread_fence_acq_rel() plasma_membar_rmw_acq_rel()
#define plasma_membar_atomic_thread_fence_seq_cst() plasma_membar_seq_cst()

#include "plasma_attr.h"

#ifdef __cplusplus
#if __cplusplus >= 201103L \
 || HAVE_ATOMIC
 /* XXX: ... add atomic header detection for other C++ compilers ... */
#include <atomic>
#endif
#else
#if __STDC_VERSION__ >= 201112L \
 || HAVE_STDATOMIC_H
 /* stdatomic.h does not appear to be provided by gcc-4.7 for C
  *   http://gcc.gnu.org/ml/gcc-help/2011-01/msg00372.html
  *   discussion thread for whether compiler or libc should provide stdatomic.h:
  *   http://sourceware.org/ml/libc-alpha/2009-08/msg00025.html */
 /* XXX: ... add stdatomic.h header detection for other C compilers ... */
#ifndef __STDC_NO_ATOMICS__  /* C11 compiler can indicate absense of atomics */
#include <stdatomic.h>
#endif
#endif
#endif

/* implement atomic_thread_fence() and atomic_signal_fence() if not available
 * (ATOMIC_VAR_INIT present if C11/C++11 atomics are supported and the
 *  appropriate <stdatomic.h> (C) or <atomic> (C++) header has been included) */

#ifndef ATOMIC_VAR_INIT  /* implement C11/C++11 memory order atomic fences */

#ifndef __ATOMIC_RELAXED
#define __ATOMIC_RELAXED  0
#endif
#ifndef __ATOMIC_CONSUME
#define __ATOMIC_CONSUME  1
#endif
#ifndef __ATOMIC_ACQUIRE
#define __ATOMIC_ACQUIRE  2
#endif
#ifndef __ATOMIC_RELEASE
#define __ATOMIC_RELEASE  3
#endif
#ifndef __ATOMIC_ACQ_REL
#define __ATOMIC_ACQ_REL  4
#endif
#ifndef __ATOMIC_SEQ_CST
#define __ATOMIC_SEQ_CST  5
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum memory_order {
  memory_order_relaxed = __ATOMIC_RELAXED,
  memory_order_consume = __ATOMIC_CONSUME,
  memory_order_acquire = __ATOMIC_ACQUIRE,
  memory_order_release = __ATOMIC_RELEASE,
  memory_order_acq_rel = __ATOMIC_ACQ_REL,
  memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

#ifdef __cplusplus
}
#endif

#if __has_extension(c_atomic) || __has_extension(cxx_atomic)

/*(same __has_extension() tests as above; will not reach if have atomic hdrs)*/
/* http://clang.llvm.org/docs/LanguageExtensions.html#__c11_atomic */
#define atomic_signal_fence(order)  __c11_atomic_signal_fence(order)
#define atomic_thread_fence(order)  __c11_atomic_thread_fence(order)

#elif __GNUC_PREREQ(4,7)

#define atomic_thread_fence(order)  __atomic_thread_fence(order)
#define atomic_signal_fence(order)  __atomic_signal_fence(order)

#else

#ifndef atomic_signal_fence
#define atomic_signal_fence(order) \
  do { if ((order) != memory_order_relaxed) plasma_membar_ccfence(); } while (0)
#endif

#ifndef atomic_thread_fence
#define atomic_thread_fence(order)                                            \
  do {                                                                        \
    switch (order) {                                                          \
      default: /*(bad input; deliver proper behavior with strongest barrier)*/\
      case memory_order_seq_cst: plasma_membar_atomic_thread_fence_seq_cst(); \
                                 break;                                       \
      case memory_order_acq_rel: plasma_membar_atomic_thread_fence_acq_rel(); \
                                 break;                                       \
      case memory_order_release: plasma_membar_atomic_thread_fence_release(); \
                                 break;                                       \
      case memory_order_acquire: plasma_membar_atomic_thread_fence_acquire(); \
                                 break;                                       \
      case memory_order_consume: plasma_membar_atomic_thread_fence_consume(); \
      case memory_order_relaxed: break;                                       \
    }                                                                         \
  } while (0)
#endif

#endif

#endif /* ATOMIC_VAR_INIT (implement C11/C++11 memory order atomic fences) */




#ifdef __cplusplus
extern "C" {
#endif

/*#include "plasma_attr.h"*/

/* (static inline functions for C99, if present, need to be in an
 *  extern "C" block and/or hidden from C++ or reformated for C++) */

#ifdef __cplusplus
}
#endif


#endif




/* NOTES and REFERENCES
 *
 *
 * http://en.wikipedia.org/wiki/Memory_ordering
 * http://en.wikipedia.org/wiki/Memory_barrier
 *
 * http://www.kernel.org/doc/Documentation/memory-barriers.txt
 *
 * http://en.cppreference.com/w/c/atomic
 * http://en.cppreference.com/w/c/atomic/memory_order
 * http://en.cppreference.com/w/cpp/atomic
 * http://en.cppreference.com/w/cpp/atomic/memory_order
 *
 *
 * Intel(R) 64 and IA-32 Architectures Developer's Manual: Vol. 3A 
 * http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html
 * http://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf
 * Chapter 8 Multiple-Processor Management
 * 8.1 Locked Atomic Operations
 * 8.2 Memory Ordering
 * 8.2.3.2 Neither Loads Nor Stores Are Reordered with Like Operations
 * 8.2.3.3 Stores Are Not Reordered With Earlier Loads
 * 8.2.3.4 Loads May Be Reordered with Earlier Stores to Different Locations
 * 8.2.3.5 Intra-Processor Forwarding Is Allowed
 * 8.2.3.6 Stores Are Transitively Visible
 * 8.2.3.7 Stores Are Seen in a Consistent Order by Other Processors
 * 8.2.3.8 Locked Instructions Have a Total Order
 * 8.2.3.9 Loads and Stores Are Not Reordered with Locked Instructions
 * 8.2.4 Fast-String Operation and Out-of-Order Stores
 *   Section 7.3.9.3 of Intel(R) 64 and IA-32 Architectures Software Developer's
 *   Manual, Volume 1 described an optimization of repeated string operations
 *   called fast-string operation.  As explained in that section, the stores
 *   produced by fast-string operation may appear to execute out of order.
 *   Software dependent upon sequential store ordering should not use string
 *   operations for the entire data structure to be stored. Data and semaphores
 *   should be separated. Order-dependent code should write to a discrete
 *   semaphore variable after any string operations to allow correctly ordered
 *   data to be seen by all processors. Atomicity of load and store operations
 *   is guaranteed only for native data elements of the string with native data
 *   size, and only if they are included in a single cache line.
 * 8.2.5 Strengthening or Weakening the Memory-Ordering Model
 *       SFENCE, LFENCE, MFENCE
 * 8.6 Detecting Hardware Multi-Threading Support and Topology
 *     Hardware Multi-Threading feature flag (CPUID.1:EDX[28] = 1)
 *     -- Indicates when set that the physical package is capable of
 *     supporting Intel Hyper-Threading Technology and/or multiple cores.
 * 8.10.2 PAUSE Instruction
 * 8.10.6.1 Use the PAUSE Instruction in Spin-Wait Loops
 * 8.10.6.7 Place Locks and Semaphores in Aligned, 128-Byte Blocks of Memory
 *
 *
 * AMD64 Architecture Programmer's Manual Volume 2: System Programming
 * http://support.amd.com/us/Processor_TechDocs/24593_APM_v2.pdf
 * 7 Memory System
 * Table 7-3. Memory Access Ordering Rules  (page 176)
 * http://developer.amd.com/resources/documentation-articles/developer-guides-manuals/
 * Note: Intel and AMD both do not reorder stores to cacheable memory, so store
 * release barrier is implemented as a simple compile barrier.  Besides the bug
 * in the PentiumPro (errata #51) (P6 is very old 32-bit chip at this point),
 * there are not any x86 chips of which this author is aware that perform
 * x86 OOStore to cacheable memory.  Similarly, loads are not reorder before
 * other loads to cacheable memory, so load acquire barrier is implemented as
 * a simple compile barrier, too.  However, in cases where a store to shared
 * memory precedes a load from shared memory and order is signficant, e.g. for
 * sequential consistency, a barrier (mfence) is required for StoreLoad ordering
 *
 * Note on x86 memory ordering  (Ed: note describes write back (WB) mem region)
 * http://nxr.netbsd.org/xref/src/sys/arch/x86/include/lock.h
 *
 *
 * ARMv7 Architecture Reference Manual  (requires free account)
 * http://infocenter.arm.com/help/topic/com.arm.doc.ddi0403c/index.html
 * ARMv8 Instruction Set Overview       (requires free account)
 * http://infocenter.arm.com/help/topic/com.arm.doc.genc010197a/index.html
 * In what situations might I need to insert memory barrier instructions?
 * http://infocenter.arm.com/help/topic/com.arm.doc.faqs/ka14041.html
 * Barrier Litmus Tests and Cookbook    (download PDF at URL)
 * http://infocenter.arm.com/help/topic/com.arm.doc.genc007826/index.html
 * Memory access ordering
 * http://blogs.arm.com/software-enablement/431-memory-access-ordering-an-introduction/
 * http://blogs.arm.com/software-enablement/448-memory-access-ordering-part-2-barriers-and-the-linux-kernel/
 * http://blogs.arm.com/software-enablement/594-memory-access-ordering-part-3-memory-access-ordering-in-the-arm-architecture/
 * http://forums.arm.com/index.php?/topic/15079-what-is-the-difference-between-inner-cache-and-outer-cache/
 *
 *
 * http://www.oracle.com/technetwork/server-storage/solarisstudio/documentation/oss-compiler-barriers-176055.pdf
 * http://www.oracle.com/technetwork/server-storage/solarisstudio/documentation/oss-memory-barriers-fences-176056.pdf
 * http://www.opensparc.net/docs/UA2007-current-draft-HP-EXT.pdf
 * 6 Instruction Set Overview
 * 6.3 Instruction Categories
 * 6.3.1 Memory Access Instructions
 *   Programming A SPARC V9 program containing self-modifying code should
 *   Note use FLUSH instruction(s) after executing stores to modify
 *   instruction memory and before executing the modified
 *   instruction(s), to ensure the consistency of program execution.
 * 6.3.2 Memory Synchronization Instructions
 * 6.3.6.5 Flush Windows Instruction
 * Oracle SPARC Architecture 2011
 * http://www.oracle.com/technetwork/systems/opensparc/sparc-architecture-2011-1728132.pdf
 * http://www.opensparc.net/offers/OpenSPARC_Internals_Book.pdf
 * Memory Ordering in Modern Microprocessors, Part II
 * http://conix.dyndns.org/ljarchive/LJ/137/8212.html
 * http://www.sparc.org/standards/SPARCV9.pdf  Chapter 8 Memory Models
 *
 *
 * PowerPC storage model and AIX programming
 * http://www.ibm.com/developerworks/systems/articles/powerpc.html
 *
 * PowerISA_V2.06B_V2_PUBLIC.pdf
 *
 * Example POWER Implementation for C/C++ Memory Model
 * http://www.rdrop.com/users/paulmck/scalability/paper/N2745r.2011.03.04a.html
 * http://www.rdrop.com/users/paulmck/scalability/paper/N2745rP5.2010.02.19a.html
 *
 *
 * A Formal Specification Intel(R) Itanium(R) Processor Family Memory Ordering
 * http://www.intel.com/design/itanium/downloads/251429.htm
 *
 *
 * Unsupported hardware (very old chips and very old chips with bugs):
 *
 * Ancient hardware probably costs more money to run (power and cooling) than
 * the processing power it provides, compared to more recent hardware.
 * Please consider TCO before griping about the following unsupported hardware.
 *
 * - 32-bit SPARC (support not attempted)
 * - 32-bit POWER (support not attempted)
 * - 32-bit ARMv6 (support not attempted) or any ARM chip <= ARMv6
 *
 * - SPARC UltraSPARC Spitfire (1995) and UltraSPARC IIs Blackbird (1997)
 *   http://en.wikipedia.org/wiki/SPARC
 *     1995  UltraSPARC (Spitfire)
 *     1997  UltraSPARC IIs (Blackbird)
 * https://github.com/joyent/illumos-joyent/blob/master/usr/src/common/atomic/sparcv9/atomic.s
 *   "Spitfires and Blackbirds have a problem with membars in the delay slot
 *    (SF_ERRATA_51)."
 *
 * - Intel Pentium Pro (P6) (1995)
 *   http://en.wikipedia.org/wiki/Pentium_Pro
 *   out-of-order store (OOStore) due to bug; x86 otherwise strongly ordered
 *   http://download.intel.com/design/archives/processors/pro/docs/24268935.pdf
 *   errata 51
 *     UC store may be reordered around WC transaction
 *   errata 66
 *     Delayed line invalidation issue during 2-way MP data ownership transfer
 *   errata 92
 *     Potential loss of data coherency during MP data ownership transfer
 *
 *
 * minimum memory barriers effectively provided by plasma_membar_* macros herein
 * when macros are used in correct context.  (e.g. plasma_membar_ld_datadep() to
 * load dependent data: between loading pointer and loading dependent data
 * accessed through the pointer, or plasma_membar_ld_ctrldep() used as barrier
 * in ld-cmp-br-barrier instruction sequence prior to a load or store)
 *
 * plasma_membar_ccfence()                                                (none)
 * plasma_membar_LoadLoad()     LoadLoad
 * plasma_membar_StoreStore()                          StoreStore
 * plasma_membar_ld_consumer()  LoadLoad
 * plasma_membar_st_producer()                         StoreStore
 * plasma_membar_ld_datadep()   LoadLoad
 * plasma_membar_ld_ctrldep()   LoadLoad | LoadStore
 * plasma_membar_ld_acq()       LoadLoad | LoadStore
 * plasma_membar_st_rel()                  LoadStore | StoreStore
 * plasma_membar_rmw_acq_rel()  LoadLoad | LoadStore | StoreStore
 * plasma_membar_seq_cst()      LoadLoad | LoadStore | StoreStore | StoreLoad
 *
 * (some barriers might be implemented more strongly on some platforms)
 *
 * A StoreLoad barrier is needed when the synchronization point between two
 * threads is a store instruction, e.g. a lock, and all subsequent memory
 * operations -- both loads and stores -- must occur after the store.  To be
 * more explicit, on POWER and ARM, an 'exchange' type operation is not a
 * single CAS (compare-and-swap) instruction, but rather a sequence of
 * LL/SC (load-linked/store conditional) instructions.  Subsequent loads must
 * not occur until the store that obtains the lock is visible to all other
 * processors.  On ARM and POWER, a full barrier (dmb (ARM) or sync (POWER))
 * provides StoreLoad, though an alternative means to obtain a StoreLoad
 * barrier on ARM or POWER is a sequence of instructions (ARM: ld-cmp-br-isb)
 * (POWER: ld-cmp-br-isync) following the LL/SC.  This is true only when used
 * in conjunction with LL/SC instructions.
 *
 *
 * Additional Notes
 *
 *
 * Read of misaligned data is not atomic.  Always use natural alignment
 * (or larger alignment) for data on which atomics are performed.
 *
 * Variables on which we operate between multiple threads should be _Atomic
 * (where available), or at least volatile. in order to communicate to compiler
 * that it should not potentially generate code that uses intermediates, which
 * might introduce race conditions.  However, even that differs between
 * compilers.  HP aCC adds acquire and release memory barriers to all ld and st
 * memory accesses on volatile variables and has multiple (non-standard)
 * volatile declarations to partially relax the barriers emitted by compiler.
 *
 * Implementation note (specifically regarding plasma_membar_ld_datadep()):
 * Macros here are implemented to take zero or more parameters (#define foo()),
 * instead of bare token substitution, and are intended to be used as statements
 * (e.g. 'foo();').  If a macro expands to nothing, then the subsequent ';'
 * statement might result in a compiler warning for an empty statement.  One
 * workaround (employed within) is to expand the macro to 'do { } while (0)'
 * which avoids the compiler warning and will be optimized away by the compiler.
 * More insidious, if the macro which expands to multiple statements is used in
 * an if-else that does not use brackets (if { } else { }), then the behavior of
 * the program might change silently.  'if (x) foo();' with foo() expanding to
 * xyx(); abc() becomes the following, and abc() gets executed unconditionally:
 *   if (x)
 *     xyx();
 *   abc();
 * Wrapping multiple statements in macro expansion in do { } while (0) protects
 * against such unwanted behavior.
 *
 * A note about optimizing barriers for current hardware: if a binary is created
 * on an architecture that has implemented relatively strong memory ordering
 * -- and the memory barriers take advantage of this -- but on whose
 * architecture documentation permits looser guarantees, then that same binary
 * might not run correctly on a future CPU with more relaxed memory ordering.
 * In general, plasma_membar_* barriers try to follow architecture documentation
 * and exceptions are documented.  For example, Solaris on sparcv9 runs in
 * TSO (total store order) and the memory barriers are simplified.
 * A different solution to have the correct barriers per platform is to call
 * subroutines provided by the platform, as is done with Sun Studio 12.2 and
 * above.  The slight overhead of the subroutine call is still often less than
 * taking an unnecessarily heavyweight barrier.
 *
 * http://en.cppreference.com/w/c/atomic/memory_order
 * "Sequential ordering is necessary for many multiple producer-multiple
 *  consumer situations where all consumers must observe the actions of all
 *  producers occurring in the same order."
 * (i.e. more than two threads and all CPUs must see updates in *same* order)
 *
 * http://www.rdrop.com/users/paulmck/scalability/paper/whymb.2010.07.23a.pdf
 * "One could place an smp rmb() primitive between the pointer fetch and
 *  dereference. However, this imposes unneeded overhead on systems (such as
 *  i386, IA64, POWER, and SPARC) that respect data dependencies on the read
 *  side.  A smp read barrier depends() primitive has been added to the Linux
 *  2.6 kernel to eliminate overhead on these systems."
 * [...]  
 * "One difference between the ARMv7 and the POWER memory models is that while
 *  POWER respects both data and control dependencies, ARMv7 respects only data
 *  dependencies."
 * [...]
 * "ARM allows cache coherence to have one of three scopes: single processor,
 *  a subset of the processors ("inner") and global ("outer")."
 *
 * http://www.1024cores.net/home/lock-free-algorithms/so-what-is-a-memory-model-and-how-to-cook-it/visibility
 * "However, in practice it's [visibility] the most boring property, because on cache-coherent architectures (read - on all modern commodity architectures - IA-32, Intel 64, IA-64, SPARC, POWER) visibility is ensured automatically. Namely,  each write is automatically propagated to all other processors/cores in a best-effort manner. There are no ways to prevent nor to speed it up. Period."
 *
 * http://preshing.com/20120913/acquire-and-release-semantics
 * Having said that, the strong ordering guarantees of x86/64 go out the window when you do certain things, which are also documented in the same section of Intel's docs:
 *     Marking memory as non-cacheable write-combined (for example using VirtualProtect on Windows or mmap on Linux); something only driver developers normally do.  [Ed: e.g. using mmap for memory-mapped device I/O to VRAM]
 *     Using fancy SSE instructions like movntdq or "string" instructions like rep movs. If you use those, you lose StoreStore ordering on the processor and can only get it back using an sfence instruction.
 *
 *
 * https://www.power.org/documentation/power-instruction-set-architecture-version-2-06/attachment/powerisa_v2-06b_v2_public/  (requires free account)
 * Because stores cannot be performed "out-of-order" (see Book III), if a Store instruction depends on the value returned by a preceding Load instruction (because the value returned by the Load is used to compute either the effective address specified by the Store or the value to be stored), the corresponding storage accesses are performed in program order. The same applies if whether the Store instruction is executed depends on a conditional Branch instruction that in turn depends on the value returned by a preceding Load instruction.
 * Because an isync instruction prevents the execution of instructions following the isync until instructions preceding the isync have completed, if an isync follows a conditional Branch instruction that depends on the value returned by a preceding Load instruction, the load on which the Branch depends is performed before any loads caused by instructions following the isync.  This applies even if the effects of the "dependency" are independent of the value loaded (e.g., the value is compared to itself and the Branch tests the EQ bit in the selected CR field), and even if the branch target is the sequentially next instruction.
 * With the exception of the cases described above and earlier in this section, data dependencies and control dependencies do not order storage accesses.
 *
 * B.2.1.1 Acquire Lock and Import Shared Storage
 * If the shared data structure is in storage that is neither Write Through Required nor Caching Inhibited, an lwsync instruction can be used instead of the isync instruction. If lwsync is used, the load from "data1" may be performed before the stwcx.. But if the stwcx. fails, the second branch is taken and the lwarx is re-executed. If the stwcx. succeeds, the value returned by the load from "data1" is valid even if the load is performed before the stwcx., because the lwsync ensures that the load is performed after the instance of the lwarx that created the reservation used by the successful stwcx..
 *
 * Above excerpts from POWER documentation convey
 * - load of data where the second load has a true data dependency on first load
 *   is performed in program order and does not need to be preceded by a memory
 *   barrier to enforce program ordering; plasma_membar_ld_datadep() is no-op
 * - load-compare-branch-isync sequence (where compare must depend on the load)
 *   can be used instead of full __sync to provide ordering of memory access
 *   following the load (provides LoadLoad, LoadStore barriers).
 * - An __lwsync following a LL/SC (load-linked/store-conditional) can be used
 *   to provide ordering of memory access, including the effects of a StoreLoad
 *   barrier, substituting for a full __sync() or an ld-cmp-br-isync sequence.
 *   The __lwsync must follow a LL/SC sequence of instructions for this to hold.
 * Relevance/usage: full barrier (including StoreLoad barrier) is needed when
 * entering a critical section, e.g. obtaining a mutex, and the above provides
 * __lwsync after LL/SC as an alternative to a full __sync().
 *
 * Performance: prefer lwsync instead of isync when using LL/SC instructions
 *   [PATCH 6/6] powerpc: Use lwsync for acquire barrier if CPU supports it
 *   https://lists.ozlabs.org/pipermail/linuxppc-dev/2010-February/080354.html
 * Using lwsync in place of isync after LL/SC increases performance on
 * IBM POWER, but has negative performance implications on PA6T.  PA6T is
 * compliant with POWER v2.04 ISA, and is no longer being developed, but is
 * still manufactured for the US Government according to
 * http://en.wikipedia.org/wiki/PWRficient
 *
 * http://sites.google.com/site/peeterjoot/math2009/atomic.pdf
 * POWER5 is weakly ordered; POWER6 is not; POWER7 is again weakly ordered
 *
 *
 * Instruction synchronization asm on ARM is 'isb', on POWER is 'isync'.  When
 * used properly, instruction syncronization can provide acquire semantics.  An
 * instruction synchronization barrier flushes the instruction pipeline and
 * refetches instructions.
 *
 * For background why read-side barrier is needed:
 * http://blogs.msdn.com/b/itgoestoeleven/archive/2008/03/11/the-joys-of-compiler-and-processor-reordering-why-you-need-the-read-side-barrier.aspx
 * Then, it might be easier to see how instruction synchronization can be
 * employed to implement LoadLoad and LoadStore barriers.
 *
 * A Tutorial Introduction to the ARM and POWER Relaxed Memory Models
 * http://www.cl.cam.ac.uk/~pes20/ppc-supplemental/test7.pdf  (Oct 2012)
 *   4.3 Control-isb/isync Dependencies
 *
 * isb/isync must be used in ld-cmp-br-isync sequence where the ld-cmp-br is
 * on the memory value whose load completion is the synchronization pivot point,
 * as the load must complete before the comparison and branch can be committed.
 * Instruction synchronization after the branch has the effect that subsequent
 * loads are not satisfied until after the branch instruction is committed.
 *
 * On ARM, plasma_membar_ld_ctrldep(), which uses 'isb', can be a better choice
 * over the more expensive 'dmb' used in plasma_membar_ld_acq().  On POWER,
 * plasma_membar_ld_ctrldep() could be implemented with 'isync', but is instead
 * implemented with 'lwsync' since lwsync can be faster than isync on modern
 * POWER hardware.
 *   (https://lists.ozlabs.org/pipermail/linuxppc-dev/2010-February/080354.html)
 *   (http://patchwork.ozlabs.org/patch/45013/)
 *
 * As an aside, C11 atomic_thread_fence() does not provide an interface to
 * instruction synchronization -- C11 only provides (potentially weaker) acquire
 * or (potentially stronger) sequential consistency barriers.
 *
 *
 * Atomic operations on some architectures imply additional barriers and
 * therefore explicit barriers do not need to be used alongside these atomic
 * ops to protect non-atomic loads and stores.  plasma_membar_rmw_acq_rel()
 * is the barrier to use with atomic operations, unless sequential consistency
 * is required (in which case, use plasma_membar_seq_cst()).
 *   http://g.oswego.edu/dl/jmm/cookbook.html
 *   http://en.cppreference.com/w/c/atomic/memory_order
 *   http://en.wikipedia.org/wiki/Memory_ordering
 *
 *
 * To obtain predictable behavior between threads sharing data or processes
 * sharing data (e.g. in shared memory), a valid pairing mechanism of ordering
 * and synchronization must be visible to both compiler and hardware processors.
 * The compiler must be aware so that the compiler does not optimize the code to
 * perturb the ordering, and the processor must be aware so that the processor
 * does not reorder critical instructions or violate intended memory ordering by
 * speculating across the chosen synchronization points.
 *
 * - producer and consumer
 * - acquire and release
 * - acq_rel
 *   (after read-modify-write (rmw) ops)
 * - sequential consistency
 *   (three or more threads that need to see changes in the same order even when
 *    updates are made by two or more threads, each to a *different* location)
 *
 *
 * Additional References  (in no particular order)
 *
 *
 * The JSR-133 Cookbook for Compiler Writers (Dou Lea and JMM mailing list)
 * http://g.oswego.edu/dl/jmm/cookbook.html
 *
 * Understanding POWER Multiprocesors
 * http://www.cl.cam.ac.uk/~pes20/ppc-supplemental/
 * http://www.cl.cam.ac.uk/~pes20/weakmemory/index.html
 * The Semantics of x86 Multiprocessor Programs
 * http://www.cl.cam.ac.uk/~pes20/weakmemory/index3.html
 * Clarifying and Compiling C/C++ Concurrency: from C++11 to POWER
 * http://www.cl.cam.ac.uk/~pes20/cppppc/
 * Synchronising C/C++ and POWER
 * http://www.cl.cam.ac.uk/~pes20/cppppc-supplemental/
 * http://www.cl.cam.ac.uk/~pes20/cppppc-supplemental/pldi010-sarkar.pdf
 * A Tutorial Introduction to the ARM and POWER Relaxed Memory Models
 * http://www.cl.cam.ac.uk/~pes20/ppc-supplemental/test7.pdf  (Oct 2012)
 *   (great read, but note that some artificial dependencies must occur in
 *    assembly code so that compiler does not optimize away the artificial dep)
 * An Axiomatic Memory Model for POWER Multiprocessors
 * http://acg.cis.upenn.edu/papers/cav12_axiomatic_power.pdf
 *
 * Shared Memory Consistency Models: A Tutorial
 * ftp://gatekeeper.dec.com/pub/DEC/WRL/research-reports/WRL-TR-95.7.pdf
 * Memory Consistency Models for Shared-Memory Multiprocessors
 * http://www.hpl.hp.com/techreports/Compaq-DEC/WRL-95-9.pdf
 *
 * http://kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.2011.01.02a.pdf
 * http://www.rdrop.com/users/paulmck/scalability/paper/ordering.2007.09.19a.pdf
 * http://www.rdrop.com/users/paulmck/scalability/paper/whymb.2010.06.07c.pdf
 * http://sourceforge.net/p/predef/wiki/Home/
 * http://www.hpl.hp.com/personal/Hans_Boehm/c++mm/ordering_integrated.html
 * http://www.hpl.hp.com/personal/Hans_Boehm/c++mm/dependencies.html
 * http://www.hpl.hp.com/research/linux/atomic_ops/
 * Things You Never Wanted to Know About Memory Fences
 * http://www.nwcpp.org/Downloads/2008/Memory_Fences.pdf
 * http://www.stdthread.co.uk/forum/index.php?topic=72.0
 * http://www.developerfusion.com/article/138018/memory-ordering-for-atomic-operations-in-c0x/
 * http://peeterjoot.wordpress.com/2009/11/29/an-attempt-to-illustrate-differences-between-memory-ordering-and-atomic-access/
 *   (see also references at end of article)
 * http://www.domaigne.com/blog/computing/mutex-and-memory-visibility/
 * http://www.cs.nmsu.edu/~pfeiffer/classes/573/notes/consistency.html
 *
 * Well written set of articles can be found at 1024cores.net.
 * NB: article is somewhat x86-centric and some coding samples should be
 * carefully reviewed to ensure proper barriers are present for other
 * architectures.
 * http://www.1024cores.net/home/lock-free-algorithms
 * http://www.1024cores.net/home/lock-free-algorithms/introduction
 *   wait-free, lock-free, blocking definitions
 * http://www.1024cores.net/home/lock-free-algorithms/first-things-first
 * http://www.1024cores.net/home/lock-free-algorithms/your-arsenal
 * http://www.1024cores.net/home/lock-free-algorithms/so-what-is-a-memory-model-and-how-to-cook-it
 * http://www.1024cores.net/home/lock-free-algorithms/scalability-prerequisites
 * http://www.1024cores.net/home/lock-free-algorithms/false-sharing---false
 * http://www.1024cores.net/home/lock-free-algorithms/reader-writer-problem
 * http://www.1024cores.net/home/lock-free-algorithms/queues
 * http://www.1024cores.net/home/lock-free-algorithms/lazy-concurrent-initialization
 * http://www.1024cores.net/home/lock-free-algorithms/object-life-time-management
 * http://www.1024cores.net/home/lock-free-algorithms/tricks
 *
 * http://preshing.com/20120612/an-introduction-to-lock-free-programming
 * http://preshing.com/20120625/memory-ordering-at-compile-time
 * http://preshing.com/20120913/acquire-and-release-semantics
 * http://preshing.com/20121019/this-is-why-they-call-it-a-weakly-ordered-cpu
 * http://preshing.com/20120515/memory-reordering-caught-in-the-act
 * http://preshing.com/20120226/roll-your-own-lightweight-mutex
 * http://preshing.com/20120305/implementing-a-recursive-mutex
 *
 * example shows Peterson lock which demonstrates when StoreLoad barrier needed
 * http://bartoszmilewski.com/2008/11/05/who-ordered-memory-fences-on-an-x86/
 * http://bartoszmilewski.com/2008/08/04/multicores-and-publication-safety/
 * http://bartoszmilewski.com/2008/12/01/c-atomics-and-memory-ordering/
 *
 * http://gcc.gnu.org/wiki/Atomic
 * http://gcc.gnu.org/wiki/Atomic/GCCMM/CodeGen
 * http://gcc.gnu.org/projects/cxx0x.html
 * http://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html#_005f_005fsync-Builtins
 * http://llvm.org/docs/Atomics.html
 * http://llvm.org/docs/LangRef.html
 * OSAtomic.h on Mac OSX
 * https://developer.apple.com/library/mac/#documentation/System/Reference/OSAtomic_header_reference/Reference/reference.html#//apple_ref/doc/uid/TP40011482
 *
 * Additional reference links are inline with macros in code comments above.
 */
