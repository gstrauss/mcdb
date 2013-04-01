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

#include "plasma_membar.h"

/* spinlocks / busy-wait loops that pause briefly (versus spinning on nop
 * instructions) benefit modern processors by reducing memory hazards upon loop
 * exit as well as potentially saving CPU power and reducing resource contention
 * with other CPU threads (SMT) (a.k.a strands) on the same core. */

#if defined(_MSC_VER)

  /* prefer using intrinsics directly instead of winnt.h macro */
  #include <intrin.h>
  #if defined(__x86_64__) || defined(__i386__)
  #pragma intrinsic(_mm_pause)
  #define plasma_spin_pause()  _mm_pause()
  #elif defined(__ia64__)
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

  /* http://software.intel.com/en-us/forums/topic/309231
   * http://siyobik.info.gf/main/reference/instruction/PAUSE
   * http://stackoverflow.com/questions/7086220/what-does-rep-nop-mean-in-x86-assembly
   * http://stackoverflow.com/questions/7371869/minimum-time-a-thread-can-pause-in-linux
   */
  #ifdef __clang__
  #include <xmmintrin.h>
  #define plasma_spin_pause()  _mm_pause()
  #else  /* !__clang__ */
  #define plasma_spin_pause()  __asm__ __volatile__ ("pause":::"memory")
  /* (if pause not supported by older x86 assembler, "rep; nop" is equivalent)*/
  /*#define plasma_spin_pause()  __asm__ __volatile__ ("rep; nop":::"memory")*/
  #endif /* !__clang__ */

#elif defined(__ia64__)

  #define plasma_spin_pause()  __asm__ __volatile__ ("hint @pause":::"memory")

#elif defined(__arm__)

  /* ARMv7 Architecture Reference Manual (for YIELD) */
  /* ARM Compiler toolchain Compiler Reference (for __yield() instrinsic) */
  #ifdef __CC_ARM
  #define plasma_spin_pause()  __yield()
  #else
  #define plasma_spin_pause()  __asm__ __volatile__ ("yield":::"memory")
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
  #define plasma_spin_pause()  __asm __volatile__ ("rd %%ccr, %%g0 \n\t " \
                                                   "rd %%ccr, %%g0 \n\t " \
                                                   "rd %%ccr, %%g0":::"memory")

#elif defined(__ppc__)   || defined(_ARCH_PPC)  || \
      defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER)

  /* http://stackoverflow.com/questions/5425506/equivalent-of-x86-pause-instruction-for-ppc
   * https://www.power.org/documentation/power-instruction-set-architecture-version-2-06/attachment/powerisa_v2-06b_v2_public/  (requires free account)
   * 3.2 "or" Instruction
   *   "or" Shared Resource Hints
   */
  #define plasma_spin_pause()  __asm__ __volatile__ ("yield":::"memory")

#else

  /* help prevent compiler from optimizing loop away; not a pause */
  #define plasma_spin_pause()  plasma_membar_ccfence()

#endif




#ifdef __cplusplus
extern "C" {
#endif

/*#include "plasma_attrs.h"*/

/* (static inline functions for C99, if present, need to be in an
 *  extern "C" block and/or hidden from C++ or reformated for C++) */

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
 */
