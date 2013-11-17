/*
 * plasma_atomic.t.c - plasma_atomic.[ch] tests
 *
 * Copyright (c) 2013, Glue Logic LLC. All rights reserved. code()gluelogic.com
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

/*
 * TODO: lots more work needed here, but this is a fine start
 * TODO: optional interfaces in plasma_atomic.h to cast inputs, quell compiler
 *       type mismatch warnings -- only sizeof(type) matters for atomics
 *       (discard type-safety and assume arguments are appropriate sizes)
 *       (NB: casting to different type and dereferencing
 *        violates strict aliasing rules, so look into ways to communicate
 *        this to compiler, such as gcc __attribute__((may_alias)) )
 */

/* note: tests int32_t and int64_t sizes; char and short not tested below */

/* note: plasma_atomic.t.c fails to compile with (xlC 10.1) xlc_r -q32 -O2
 * (xlc generates negative .line in asm (.line -4) (compile with -S to see)) */

#include "../plasma_atomic.h"
#include "../plasma_attr.h"
#include "../plasma_membar.h"
#include "../plasma_stdtypes.h"
#include "../plasma_sysconf.h"
#include "../plasma_test.h"

#ifdef PLASMA_FEATURE_POSIX
#include <unistd.h>  /* alarm() */
#endif

#if defined(__IBMC__) || defined(__IBMCPP__)
#if !defined(_LP64) && !defined(__LP64__) && !defined(_ARCH_PPC64)
#error "plasma_atomic.h 64-bit ops require: xlc -qarch=pwr5 or better 64-bit POWER arch"
#endif
#endif

#define PLASMA_ATOMIC_T_RELAXED_LOOP_ITERATIONS 64

__attribute_noinline__
static int
plasma_atomic_t_relaxed_32 (void)
{
    const int32_t i32a[] = {
      (int32_t)0x80000000 /* INT_MIN */
     ,(int32_t)0xFFFFFFFF /* -1 */
     ,(int32_t)0
     ,(int32_t)0x00000001 /*  1 */
     ,(int32_t)0x7FFFFFFF /* INT_MAX */
    };
    int32_t x32, r32;
    int i, rc = true;
    const int iters = PLASMA_ATOMIC_T_RELAXED_LOOP_ITERATIONS;

    /* (use compiler fence to attempt to thwart compiler optimization) */

    /* store and load */
    for (i=0, x32=0; i < (int)(sizeof(i32a)/sizeof(int32_t)); ++i) {
        plasma_membar_ccfence();
        plasma_atomic_store_explicit(&x32, i32a[i], memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(x32 == i32a[i], i);
        plasma_membar_ccfence();
        r32 = plasma_atomic_load_explicit(&x32, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r32 == i32a[i], i);
    }

    /* (single process is running, so relaxed result is predictable) */

    /* add: 0 (identity) */
    r32 = plasma_atomic_fetch_add_u32(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == x32);

    /* add: 1 (increment) */
    for (i=0, x32=0; i < iters; ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_add_u32(&x32, 1, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r32 == x32-1, i);
    }
    rc &= PLASMA_TEST_COND(x32 == iters);

    /* add: 10 (other) */
    for (i=0, x32=0; i < iters; ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_add_u32(&x32, 10, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r32 == x32-10, i);
    }
    rc &= PLASMA_TEST_COND(x32 == iters*10);

    /* sub: 0 (identity) */
    r32 = plasma_atomic_fetch_sub_u32(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == x32);

    /* sub: 1 (increment) */
    for (i=0, x32=iters; i < iters; ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_sub_u32(&x32, 1, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r32 == x32+1, i);
    }
    rc &= PLASMA_TEST_COND(x32 == 0);

    /* sub: 10 (other) */
    for (i=0, x32=iters*10; i < iters; ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_sub_u32(&x32, 10, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r32 == x32+10, i);
    }
    rc &= PLASMA_TEST_COND(x32 == 0);

    /* or: 0 (identity) */
    r32 = x32 = 0xFFFFFFFF;
    (void)plasma_atomic_fetch_or_u32(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == x32);

    /* or: loop over each bit */
    for (i=0, x32=0; i < (int)(sizeof(x32)<<3); ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_or_u32(&x32, (1<<i), memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((r32|(1<<i)) == x32, i);
    }
    rc &= PLASMA_TEST_COND(x32 == -1);

  #if !defined(__IBMC__) || !defined(__OPTIMIZE__) || !(__OPTIMIZE__ == 3)
    /* XXX: xlc -O3 bug optimizes away load-and-reserve, so no reservation taken
     *      and store-conditional always fails (and we loop on lost reservation)
     */
    /* and: 0 (result is always 0) */
    x32 = 0xFFFFFFFF;
    r32 = plasma_atomic_fetch_and_u32(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(x32 == 0);
  #endif

    /* and: loop over each bit */
    for (i=0, x32=0xFFFFFFFF; i < (int)(sizeof(x32)<<3); ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_and_u32(&x32, ~(1<<i), memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((r32&~(1<<i)) == x32, i);
    }
    rc &= PLASMA_TEST_COND(x32 == 0);

  #if !defined(__IBMC__) || !defined(__OPTIMIZE__) || !(__OPTIMIZE__ == 3)
    /* XXX: xlc -O3 bug optimizes away load-and-reserve, so no reservation taken
     *      and store-conditional always fails (and we loop on lost reservation)
     */
    /* xor: 0 (identity) */
    x32 = 0xFFFFFFFF;
    r32 = plasma_atomic_fetch_xor_u32(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == x32);
  #endif

    /* xor: loop over each bit */
    for (i=0; i < (int)(sizeof(x32)<<3); ++i) {
        x32 = 0xFFFFFFFF;
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_xor_u32(&x32, (1<<i), memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((r32^(1<<i)) == x32, i);
    }
    rc &= PLASMA_TEST_COND(x32 == 0x7FFFFFFF);

    /* exchange (swap) */
    x32 = 0;
    plasma_membar_ccfence();
    r32 = plasma_atomic_exchange_n_32(&x32, 0xFFFFFFFF, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == 0);
    r32 = plasma_atomic_exchange_n_32(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == 0xFFFFFFFF);

    /* compare and exchange
     * (if compare fails, then test will fail since only one thread updating) */
    x32 = 0;
    plasma_membar_ccfence();
    r32 = 0;
    (void)plasma_atomic_compare_exchange_n_32(&x32, &r32, 0xFFFFFFFF, 0,
                                              memory_order_relaxed,
                                              memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == 0);
    r32 = 0xFFFFFFFF;
    (void)plasma_atomic_compare_exchange_n_32(&x32, &r32, 0, 0,
                                              memory_order_relaxed,
                                              memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == 0xFFFFFFFF);
    r32 = 0xFFFFFFFF;
    (void)plasma_atomic_compare_exchange_n_32(&x32, &r32, 0, 0,
                                              memory_order_relaxed,
                                              memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 != 0xFFFFFFFF);

    return rc;
}

__attribute_noinline__
static int
plasma_atomic_t_relaxed_64 (void)
{
    const int64_t i64a[] = {
      (int64_t)0x8000000000000000uLL /* INT64_MIN */
     ,(int64_t)0xFFFFFFFF7FFFFFFFuLL /* ((int64_t)INT_MIN)-1 */
     ,(int64_t)0xFFFFFFFF80000000uLL /* INT_MIN */
     ,(int64_t)0xFFFFFFFFFFFFFFFFuLL /* -1LL */
     ,(int64_t)0uLL
     ,(int64_t)0x0000000000000001uLL /*  1LL */
     ,(int64_t)0x000000007FFFFFFFuLL /* INT_MAX */
     ,(int64_t)0x0000000080000000uLL /* ((int64_t)INT_MAX)+1 */
     ,(int64_t)0x7FFFFFFFFFFFFFFFuLL /* INT64_MAX */
    };
    int64_t x64, r64;
    int i, rc = true;
    const int iters = PLASMA_ATOMIC_T_RELAXED_LOOP_ITERATIONS;

    /* (use compiler fence to attempt to thwart compiler optimization) */

    /* store and load */
    for (i=0, x64=0; i < (int)(sizeof(i64a)/sizeof(int64_t)); ++i) {
        plasma_membar_ccfence();
        plasma_atomic_store_explicit(&x64, i64a[i], memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(x64 == i64a[i], i);
        plasma_membar_ccfence();
        r64 = plasma_atomic_load_explicit(&x64, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == i64a[i], i);
    }

    /* (single process is running, so relaxed result is predictable) */

    /* add: 0 (identity) */
    r64 = plasma_atomic_fetch_add_u64(&x64, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == x64);

    /* add: 1 (increment) */
    for (i=0, x64=0; i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_add_u64(&x64, 1, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == x64-1, i);
    }
    rc &= PLASMA_TEST_COND(x64 == iters);

    /* add: 10 (other) */
    for (i=0, x64=0; i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_add_u64(&x64, 10, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == x64-10, i);
    }
    rc &= PLASMA_TEST_COND(x64 == iters*10);

    /* add: 10000000000LL (other > UINT_MAX) */
    for (i=0, x64=0; i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_add_u64(&x64, 10000000000LL,
                                          memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == x64-10000000000LL, i);
    }
    rc &= PLASMA_TEST_COND(x64 == iters*10000000000LL);

    /* sub: 0 (identity) */
    r64 = plasma_atomic_fetch_sub_u64(&x64, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == x64);

    /* sub: 1 (increment) */
    for (i=0, x64=iters; i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_sub_u64(&x64, 1, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == x64+1, i);
    }
    rc &= PLASMA_TEST_COND(x64 == 0);

    /* sub: 10 (other) */
    for (i=0, x64=iters*10; i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_sub_u64(&x64, 10, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == x64+10, i);
    }
    rc &= PLASMA_TEST_COND(x64 == 0);

    /* sub: 10000000000LL (other > UINT_MAX) */
    for (i=0, x64=iters*10000000000LL; i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_sub_u64(&x64, 10000000000LL,
                                          memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == x64+10000000000LL, i);
    }
    rc &= PLASMA_TEST_COND(x64 == 0);

    /* or: 0 (identity) */
    r64 = x64 = (int64_t)0xFFFFFFFFFFFFFFFFuLL;
    (void)plasma_atomic_fetch_or_u64(&x64, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == x64);

    /* or: loop over each bit */
    for (i=0, x64=0; i < (int)(sizeof(x64)<<3); ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_or_u64(&x64, (1LL<<i), memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((r64|(1LL<<i)) == x64, i);
    }
    rc &= PLASMA_TEST_COND(x64 == -1LL);

  #if !defined(__IBMC__) || !defined(__OPTIMIZE__) || !(__OPTIMIZE__ == 3)
    /* XXX: xlc -O3 bug optimizes away load-and-reserve, so no reservation taken
     *      and store-conditional always fails (and we loop on lost reservation)
     */
    /* and: 0 (result is always 0) */
    x64 = (int64_t)0xFFFFFFFFFFFFFFFFuLL;
    r64 = plasma_atomic_fetch_and_u64(&x64, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(x64 == 0);
  #endif

    /* and: loop over each bit */
    for (i=0, x64=(int64_t)0xFFFFFFFFFFFFFFFFuLL; i<(int)(sizeof(x64)<<3); ++i){
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_and_u64(&x64,~(1LL<<i), memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((r64&~(1LL<<i)) == x64, i);
    }
    rc &= PLASMA_TEST_COND(x64 == 0);

  #if !defined(__IBMC__) || !defined(__OPTIMIZE__) || !(__OPTIMIZE__ == 3)
    /* XXX: xlc -O3 bug optimizes away load-and-reserve, so no reservation taken
     *      and store-conditional always fails (and we loop on lost reservation)
     */
    /* xor: 0 (identity) */
    x64 = (int64_t)0xFFFFFFFFFFFFFFFFuLL;
    r64 = plasma_atomic_fetch_xor_u64(&x64, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == x64);
  #endif

    /* xor: loop over each bit */
    for (i=0; i < (int)(sizeof(x64)<<3); ++i) {
        x64 = (int64_t)0xFFFFFFFFFFFFFFFFuLL;
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_xor_u64(&x64, (1LL<<i), memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((r64^(1LL<<i)) == x64, i);
    }
    rc &= PLASMA_TEST_COND(x64 == 0x7FFFFFFFFFFFFFFFLL);

    /* exchange (swap) */
    x64 = 0;
    plasma_membar_ccfence();
    r64 = plasma_atomic_exchange_n_64(&x64, (int64_t)0xFFFFFFFFFFFFFFFFuLL,
                                      memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == 0);
    r64 = plasma_atomic_exchange_n_64(&x64, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == (int64_t)0xFFFFFFFFFFFFFFFFuLL);

    /* compare and exchange
     * (if compare fails, then test will fail since only one thread updating) */
    x64 = 0;
    plasma_membar_ccfence();
    r64 = 0;
    (void)plasma_atomic_compare_exchange_n_64(&x64, &r64,
                                              (int64_t)0xFFFFFFFFFFFFFFFFuLL, 0,
                                              memory_order_relaxed,
                                              memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == 0);
    r64 = (int64_t)0xFFFFFFFFFFFFFFFFuLL;
    (void)plasma_atomic_compare_exchange_n_64(&x64, &r64, 0, 0,
                                              memory_order_relaxed,
                                              memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == (int64_t)0xFFFFFFFFFFFFFFFFuLL);
    r64 = (int64_t)0xFFFFFFFFFFFFFFFFuLL;
    (void)plasma_atomic_compare_exchange_n_64(&x64, &r64, 0, 0,
                                              memory_order_relaxed,
                                              memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 != (int64_t)0xFFFFFFFFFFFFFFFFuLL);
    plasma_membar_ccfence();

    return rc;
}

/* (atomic ptr: repeat tests from 32 or 64-bit atomics for matching ptr type) */
#if defined(_LP64) || defined(__LP64__) || defined(_WIN64)
__attribute_noinline__
static int
plasma_atomic_t_relaxed_ptr (void)
{
    const void * i64a[] = {
      (void *)(intptr_t)0x8000000000000000uLL /* INT64_MIN */
     ,(void *)(intptr_t)0xFFFFFFFF7FFFFFFFuLL /* ((int64_t)INT_MIN)-1 */
     ,(void *)(intptr_t)0xFFFFFFFF80000000uLL /* INT_MIN */
     ,(void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL /* -1LL */
     ,(void *)(intptr_t)0uLL
     ,(void *)(intptr_t)0x0000000000000001uLL /*  1LL */
     ,(void *)(intptr_t)0x000000007FFFFFFFuLL /* INT_MAX */
     ,(void *)(intptr_t)0x0000000080000000uLL /* ((int64_t)INT_MAX)+1 */
     ,(void *)(intptr_t)0x7FFFFFFFFFFFFFFFuLL /* INT64_MAX */
    };
    const void *x64, *r64;
    int i, rc = true;
    const int iters = PLASMA_ATOMIC_T_RELAXED_LOOP_ITERATIONS;

    /* (use compiler fence to attempt to thwart compiler optimization) */

    /* store and load */
    for (i=0, x64=0; i < (int)(sizeof(i64a)/sizeof(void *)); ++i) {
        plasma_membar_ccfence();
        plasma_atomic_store_explicit(&x64, i64a[i], memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(x64 == i64a[i], i);
        plasma_membar_ccfence();
        r64 = plasma_atomic_load_explicit(&x64, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == i64a[i], i);
    }

    /* (single process is running, so relaxed result is predictable) */

    /* add: 0 (identity) */
    r64 = plasma_atomic_fetch_add_ptr(&x64, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == x64);

    /* add: 1 (increment) */
    for (i=0, x64=0; i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_add_ptr(&x64, 1, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == (char *)x64-1, i);
    }
    rc &= PLASMA_TEST_COND(x64 == (void *)(intptr_t)iters);

    /* add: 10 (other) */
    for (i=0, x64=0; i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_add_ptr(&x64, 10, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == (char *)x64-10, i);
    }
    rc &= PLASMA_TEST_COND(x64 == (void *)(intptr_t)(iters*10));

    /* add: 10000000000LL (other > UINT_MAX) */
    for (i=0, x64=0; i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_add_ptr(&x64, 10000000000LL,
                                          memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == (char *)x64-10000000000LL, i);
    }
    rc &= PLASMA_TEST_COND(x64 == (void *)(intptr_t)(iters*10000000000LL));

    /* sub: 0 (identity) */
    r64 = plasma_atomic_fetch_sub_ptr(&x64, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == x64);

    /* sub: 1 (increment) */
    for (i=0, x64=(void *)(intptr_t)iters; i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_sub_ptr(&x64, 1, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == (char *)x64+1, i);
    }
    rc &= PLASMA_TEST_COND(x64 == 0);

    /* sub: 10 (other) */
    for (i=0, x64=(void *)(intptr_t)(iters*10); i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_sub_ptr(&x64, 10, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == (char *)x64+10, i);
    }
    rc &= PLASMA_TEST_COND(x64 == 0);

    /* sub: 10000000000LL (other > UINT_MAX) */
    for (i=0, x64=(void *)(intptr_t)(iters*10000000000LL); i < iters; ++i) {
        plasma_membar_ccfence();
        r64 = plasma_atomic_fetch_sub_ptr(&x64, 10000000000LL,
                                          memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r64 == (char *)x64+10000000000LL, i);
    }
    rc &= PLASMA_TEST_COND(x64 == 0);

    /* or: 0 (identity) */
    r64 = x64 = (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL;
    (void)plasma_atomic_fetch_or_ptr((intptr_t *)&x64, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == x64);

    /* or: loop over each bit */
    for (i=0, x64=0; i < (int)(sizeof(x64)<<3); ++i) {
        plasma_membar_ccfence();
        r64 = (void *)plasma_atomic_fetch_or_ptr((intptr_t *)&x64, (1LL<<i),
                                                 memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((void *)((intptr_t)r64|(1LL<<i)) == x64, i);
    }
    rc &= PLASMA_TEST_COND(x64 == (void *)(intptr_t)-1LL);

  #if !defined(__IBMC__) || !defined(__OPTIMIZE__) || !(__OPTIMIZE__ == 3)
    /* XXX: xlc -O3 bug optimizes away load-and-reserve, so no reservation taken
     *      and store-conditional always fails (and we loop on lost reservation)
     */
    /* and: 0 (result is always 0) */
    x64 = (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL;
    r64 = (void *)plasma_atomic_fetch_and_ptr((intptr_t *)&x64, 0,
                                              memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(x64 == 0);
  #endif

    /* and: loop over each bit */
    x64 = (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL;
    for (i=0; i < (int)(sizeof(x64)<<3); ++i) {
        plasma_membar_ccfence();
        r64 = (void *)plasma_atomic_fetch_and_ptr((intptr_t *)&x64, ~(1LL<<i),
                                                  memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((void *)((intptr_t)r64&~(1LL<<i)) == x64, i);
    }
    rc &= PLASMA_TEST_COND(x64 == 0);

  #if !defined(__IBMC__) || !defined(__OPTIMIZE__) || !(__OPTIMIZE__ == 3)
    /* XXX: xlc -O3 bug optimizes away load-and-reserve, so no reservation taken
     *      and store-conditional always fails (and we loop on lost reservation)
     */
    /* xor: 0 (identity) */
    x64 = (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL;
    r64 = (void *)plasma_atomic_fetch_xor_ptr((intptr_t *)&x64, 0,
                                              memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == x64);
  #endif

    /* xor: loop over each bit */
    for (i=0; i < (int)(sizeof(x64)<<3); ++i) {
        x64 = (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL;
        plasma_membar_ccfence();
        r64 = (void *)plasma_atomic_fetch_xor_ptr((intptr_t *)&x64, (1LL<<i),
                                                  memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((void *)((intptr_t)r64^(1LL<<i)) == x64, i);
    }
    rc &= PLASMA_TEST_COND(x64 == (void *)(intptr_t)0x7FFFFFFFFFFFFFFFLL);

    /* exchange (swap) */
    x64 = 0;
    plasma_membar_ccfence();
    r64 = plasma_atomic_exchange_n_vptr(&x64,
                                        (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL,
                                        memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == 0);
    r64 = plasma_atomic_exchange_n_vptr(&x64, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL);

    /* compare and exchange
     * (if compare fails, then test will fail since only one thread updating) */
    x64 = 0;
    plasma_membar_ccfence();
    r64 = 0;
    (void)plasma_atomic_compare_exchange_n_vptr(&x64, &r64,
                                                (void *)
                                                  (intptr_t)0xFFFFFFFFFFFFFFFFuLL,
                                                0,
                                                memory_order_relaxed,
                                                memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == 0);
    r64 = (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL;
    (void)plasma_atomic_compare_exchange_n_vptr(&x64, &r64, 0, 0,
                                                memory_order_relaxed,
                                                memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 == (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL);
    r64 = (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL;
    (void)plasma_atomic_compare_exchange_n_vptr(&x64, &r64, 0, 0,
                                                memory_order_relaxed,
                                                memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r64 != (void *)(intptr_t)0xFFFFFFFFFFFFFFFFuLL);
    plasma_membar_ccfence();

    return rc;
}
#else  /* ! (defined(_LP64) || defined(__LP64__) || defined(_WIN64)) */
__attribute_noinline__
static int
plasma_atomic_t_relaxed_ptr (void)
{
    const void * i32a[] = {
      (void *)(intptr_t)0x80000000 /* INT_MIN */
     ,(void *)(intptr_t)0xFFFFFFFF /* -1 */
     ,(void *)(intptr_t)0
     ,(void *)(intptr_t)0x00000001 /*  1 */
     ,(void *)(intptr_t)0x7FFFFFFF /* INT_MAX */
    };
    void *x32, *r32;
    int i, rc = true;
    const int iters = PLASMA_ATOMIC_T_RELAXED_LOOP_ITERATIONS;

    /* (use compiler fence to attempt to thwart compiler optimization) */

    /* store and load */
    for (i=0, x32=0; i < (int)(sizeof(i32a)/sizeof(void *)); ++i) {
        plasma_membar_ccfence();
        plasma_atomic_store_explicit(&x32, i32a[i], memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(x32 == i32a[i], i);
        plasma_membar_ccfence();
        r32 = plasma_atomic_load_explicit(&x32, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r32 == i32a[i], i);
    }

    /* (single process is running, so relaxed result is predictable) */

    /* add: 0 (identity) */
    r32 = plasma_atomic_fetch_add_ptr(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == x32);

    /* add: 1 (increment) */
    for (i=0, x32=0; i < iters; ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_add_ptr(&x32, 1, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r32 == (char *)x32-1, i);
    }
    rc &= PLASMA_TEST_COND(x32 == (void *)(intptr_t)iters);

    /* add: 10 (other) */
    for (i=0, x32=0; i < iters; ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_add_ptr(&x32, 10, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r32 == (char *)x32-10, i);
    }
    rc &= PLASMA_TEST_COND(x32 == (void *)(intptr_t)(iters*10));

    /* sub: 0 (identity) */
    r32 = plasma_atomic_fetch_sub_ptr(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == x32);

    /* sub: 1 (increment) */
    for (i=0, x32=(void *)(intptr_t)iters; i < iters; ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_sub_ptr(&x32, 1, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r32 == (char *)x32+1, i);
    }
    rc &= PLASMA_TEST_COND(x32 == 0);

    /* sub: 10 (other) */
    for (i=0, x32=(void *)(intptr_t)(iters*10); i < iters; ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_sub_ptr(&x32, 10, memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX(r32 == (char *)x32+10, i);
    }
    rc &= PLASMA_TEST_COND(x32 == 0);

    /* or: 0 (identity) */
    r32 = x32 = (void *)(intptr_t)0xFFFFFFFF;
    (void)plasma_atomic_fetch_or_ptr(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == x32);

    /* or: loop over each bit */
    for (i=0, x32=0; i < (int)(sizeof(x32)<<3); ++i) {
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_or_ptr(&x32, (1<<i), memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((void *)((intptr_t)r32|(1<<i)) == x32, i);
    }
    rc &= PLASMA_TEST_COND(x32 == (void *)(intptr_t)-1);

  #if !defined(__IBMC__) || !defined(__OPTIMIZE__) || !(__OPTIMIZE__ == 3)
    /* XXX: xlc -O3 bug optimizes away load-and-reserve, so no reservation taken
     *      and store-conditional always fails (and we loop on lost reservation)
     */
    /* and: 0 (result is always 0) */
    x32 = (void *)(intptr_t)0xFFFFFFFF;
    r32 = plasma_atomic_fetch_and_ptr(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(x32 == 0);
  #endif

    /* and: loop over each bit */
    for (i=0, x32=(void *)(intptr_t)0xFFFFFFFF; i < (int)(sizeof(x32)<<3); ++i){
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_and_ptr(&x32, ~(1<<i), memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((void *)((intptr_t)r32&~(1<<i)) == x32, i);
    }
    rc &= PLASMA_TEST_COND(x32 == 0);

  #if !defined(__IBMC__) || !defined(__OPTIMIZE__) || !(__OPTIMIZE__ == 3)
    /* XXX: xlc -O3 bug optimizes away load-and-reserve, so no reservation taken
     *      and store-conditional always fails (and we loop on lost reservation)
     */
    /* xor: 0 (identity) */
    x32 = (void *)(intptr_t)0xFFFFFFFF;
    r32 = plasma_atomic_fetch_xor_ptr(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == x32);
  #endif

    /* xor: loop over each bit */
    for (i=0; i < (int)(sizeof(x32)<<3); ++i) {
        x32 = (void *)(intptr_t)0xFFFFFFFF;
        plasma_membar_ccfence();
        r32 = plasma_atomic_fetch_xor_ptr(&x32, (1<<i), memory_order_relaxed);
        plasma_membar_ccfence();
        rc &= PLASMA_TEST_COND_IDX((void *)((intptr_t)r32^(1<<i)) == x32, i);
    }
    rc &= PLASMA_TEST_COND(x32 == (void *)(intptr_t)0x7FFFFFFF);

    /* exchange (swap) */
    x32 = 0;
    plasma_membar_ccfence();
    r32 = plasma_atomic_exchange_n_vptr(&x32, 0xFFFFFFFF, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == 0);
    r32 = plasma_atomic_exchange_n_vptr(&x32, 0, memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == (void *)(intptr_t)0xFFFFFFFF);

    /* compare and exchange
     * (if compare fails, then test will fail since only one thread updating) */
    x32 = 0;
    plasma_membar_ccfence();
    r32 = 0;
    (void)plasma_atomic_compare_exchange_n_vptr(&x32, &r32, 0xFFFFFFFF, 0,
                                                memory_order_relaxed,
                                                memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == 0);
    r32 = (void *)(intptr_t)0xFFFFFFFF;
    (void)plasma_atomic_compare_exchange_n_vptr(&x32, &r32, 0, 0,
                                                memory_order_relaxed,
                                                memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 == (void *)(intptr_t)0xFFFFFFFF);
    r32 = (void *)(intptr_t)0xFFFFFFFF;
    (void)plasma_atomic_compare_exchange_n_vptr(&x32, &r32, 0, 0,
                                                memory_order_relaxed,
                                                memory_order_relaxed);
    plasma_membar_ccfence();
    rc &= PLASMA_TEST_COND(r32 != (void *)(intptr_t)0xFFFFFFFF);

    return rc;
}
#endif /* ! (defined(_LP64) || defined(__LP64__) || defined(_WIN64)) */

__attribute_noinline__
static int
plasma_atomic_t_relaxed (void)
{
    int rc = true;
    rc &= plasma_atomic_t_relaxed_32();
    rc &= plasma_atomic_t_relaxed_64();
    rc &= plasma_atomic_t_relaxed_ptr();
    return rc;
}

typedef struct plasma_atomic_t_thr_arg {
  void *atomicptr;  /* (if more to share, put in separate shared structure) */
  void *opvalptr;   /* (if more to share, put in separate shared structure) */
  int status;
  int iters;
  enum memory_order memmodel;
} plasma_atomic_t_thr_arg;

#ifdef __cplusplus
extern "C" {
#endif

__attribute_noinline__
static void *
plasma_atomic_t_nthreads_add32 (void * const thr_arg)
{
    /* d->iters should be large enough that threads are likely to run at same
     * time on different CPU cores, instead of one completing very quickly
     * within single time slice and before other threads have chance to run */
    /* Typical usage of atomics will provide memmodel as explicit enum value
     * rather than as a variable, so optimization knows memmodel at compile
     * time.  Therefore, for testing, prefer use of explicit enum in atomics */
    plasma_atomic_t_thr_arg * const restrict d =
      (plasma_atomic_t_thr_arg *)thr_arg;
    int32_t * const restrict x32p = (int32_t *)d->atomicptr;
    int32_t r32, x32;
    const int32_t opval = *(int32_t *)d->opvalptr;
    const int iters = d->iters;
    int i, rc = true;
    plasma_test_barrier_wait();
    if (d->memmodel == memory_order_seq_cst) {
        for (i=0; i < iters && rc; ++i) {
            r32 = plasma_atomic_fetch_add_explicit(x32p, opval,
                                                   memory_order_seq_cst);
            x32 = plasma_atomic_load_explicit(x32p, memory_order_relaxed);
            rc &= PLASMA_TEST_COND(r32 < x32);
        }
    }
    else if (d->memmodel == memory_order_acq_rel) {
        for (i=0; i < iters && rc; ++i) {
            r32 = plasma_atomic_fetch_add_explicit(x32p, opval,
                                                   memory_order_acq_rel);
            x32 = plasma_atomic_load_explicit(x32p, memory_order_relaxed);
            rc &= PLASMA_TEST_COND(r32 < x32);
        }
    }
    else if (d->memmodel == memory_order_relaxed) {
        for (i=0; i < iters; ++i) {
            (void)plasma_atomic_fetch_add_explicit(x32p, opval,
                                                   memory_order_relaxed);
        }
    }
    else {
        int valid = -1;                          /* well, *not* valid */
        rc &= PLASMA_TEST_COND(d->memmodel == valid); /* will fail */
    }
    d->status = rc;
    return NULL;
}

__attribute_noinline__
static void *
plasma_atomic_t_nthreads_add64 (void * const thr_arg)
{
    /* d->iters should be large enough that threads are likely to run at same
     * time on different CPU cores, instead of one completing very quickly
     * within single time slice and before other threads have chance to run */
    /* Typical usage of atomics will provide memmodel as explicit enum value
     * rather than as a variable, so optimization knows memmodel at compile
     * time.  Therefore, for testing, prefer use of explicit enum in atomics */
    plasma_atomic_t_thr_arg * const restrict d =
      (plasma_atomic_t_thr_arg *)thr_arg;
    int64_t * const restrict x64p = (int64_t *)d->atomicptr;
    int64_t r64, x64;
    const int64_t opval = *(int64_t *)d->opvalptr;
    const int iters = d->iters;
    int i, rc = true;
    plasma_test_barrier_wait();
    if (d->memmodel == memory_order_seq_cst) {
        for (i=0; i < iters && rc; ++i) {
            r64 = plasma_atomic_fetch_add_explicit(x64p, opval,
                                                   memory_order_seq_cst);
            x64 = plasma_atomic_load_explicit(x64p, memory_order_relaxed);
            rc &= PLASMA_TEST_COND(r64 < x64);
        }
    }
    else if (d->memmodel == memory_order_acq_rel) {
        for (i=0; i < iters && rc; ++i) {
            r64 = plasma_atomic_fetch_add_explicit(x64p, opval,
                                                   memory_order_acq_rel);
            x64 = plasma_atomic_load_explicit(x64p, memory_order_relaxed);
            rc &= PLASMA_TEST_COND(r64 < x64);
        }
    }
    else if (d->memmodel == memory_order_relaxed) {
        for (i=0; i < iters; ++i) {
            (void)plasma_atomic_fetch_add_explicit(x64p, opval,
                                                   memory_order_relaxed);
        }
    }
    else {
        int valid = -1;                          /* well, *not* valid */
        rc &= PLASMA_TEST_COND(d->memmodel == valid); /* will fail */
    }
    d->status = rc;
    return NULL;
}

__attribute_noinline__
static void *
plasma_atomic_t_nthreads_addptr (void * const thr_arg)
{
    /* d->iters should be large enough that threads are likely to run at same
     * time on different CPU cores, instead of one completing very quickly
     * within single time slice and before other threads have chance to run */
    /* Typical usage of atomics will provide memmodel as explicit enum value
     * rather than as a variable, so optimization knows memmodel at compile
     * time.  Therefore, for testing, prefer use of explicit enum in atomics */
    plasma_atomic_t_thr_arg * const restrict d =
      (plasma_atomic_t_thr_arg *)thr_arg;
    intptr_t * const restrict xptrp = (intptr_t *)d->atomicptr;
    intptr_t rptr, xptr;
    const intptr_t opval = *(intptr_t *)d->opvalptr;
    const int iters = d->iters;
    int i, rc = true;
    plasma_test_barrier_wait();
    if (d->memmodel == memory_order_seq_cst) {
        for (i=0; i < iters && rc; ++i) {
            rptr = plasma_atomic_fetch_add_explicit(xptrp, opval,
                                                    memory_order_seq_cst);
            xptr = plasma_atomic_load_explicit(xptrp, memory_order_relaxed);
            rc &= PLASMA_TEST_COND(rptr < xptr);
        }
    }
    else if (d->memmodel == memory_order_acq_rel) {
        for (i=0; i < iters && rc; ++i) {
            rptr = plasma_atomic_fetch_add_explicit(xptrp, opval,
                                                    memory_order_acq_rel);
            xptr = plasma_atomic_load_explicit(xptrp, memory_order_relaxed);
            rc &= PLASMA_TEST_COND(rptr < xptr);
        }
    }
    else if (d->memmodel == memory_order_relaxed) {
        for (i=0; i < iters; ++i) {
            (void)plasma_atomic_fetch_add_explicit(xptrp, opval,
                                                   memory_order_relaxed);
        }
    }
    else {
        int valid = -1;                          /* well, *not* valid */
        rc &= PLASMA_TEST_COND(d->memmodel == valid); /* will fail */
    }
    d->status = rc;
    return NULL;
}

#ifdef __cplusplus
}
#endif

__attribute_noinline__
static int
plasma_atomic_t_nthreads_add (const int nthreads)
{
    plasma_atomic_t_thr_arg * const thr_structs =
      plasma_test_malloc(nthreads * sizeof(plasma_atomic_t_thr_arg));
    void ** const restrict thr_args =
      plasma_test_malloc(nthreads * sizeof(void *));
    union { int64_t i64; intptr_t iptr; int32_t i32; } opval;
    int64_t x64;
    void * xptr;
    int32_t x32;
    int rc = true;
    int n;
    const int iters  = 100000;
    const int addval = 1; /* increment by 1 */

    if (thr_structs == NULL || thr_args == NULL)
        PLASMA_TEST_PERROR_ABORT("plasma_test_malloc", 0);

    if (iters > (0x7FFFFFFF/(nthreads * addval)))
        PLASMA_TEST_PERROR_ABORT("tests produce out-of-range results", 0);

    for (n=0; n < nthreads; ++n)
        thr_args[n] = &thr_structs[n];

    /* note: access data through thr_args[] instead of thr_structs[] to avoid
     * falling afoul of aliasing assumptions at higher optimization levels */

    opval.i32 = addval;

    for (n=0; n < nthreads; ++n) {
        ((plasma_atomic_t_thr_arg *)thr_args[n])->atomicptr=&x32;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->opvalptr=&opval.i32;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->status  =false;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->iters   =iters;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->memmodel=memory_order_seq_cst;
    }
    x32 = 0;
    plasma_test_nthreads(nthreads,
                         plasma_atomic_t_nthreads_add32, thr_args, NULL);
    for (n=0; n < nthreads; ++n)
        rc &= ((plasma_atomic_t_thr_arg *)thr_args[n])->status;
    rc &= PLASMA_TEST_COND(x32 == (nthreads * iters * opval.i32));

    for (n=0; n < nthreads; ++n) {
        ((plasma_atomic_t_thr_arg *)thr_args[n])->status  =false;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->memmodel=memory_order_acq_rel;
    }
    x32 = 0;
    plasma_test_nthreads(nthreads,
                         plasma_atomic_t_nthreads_add32, thr_args, NULL);
    for (n=0; n < nthreads; ++n)
        rc &= ((plasma_atomic_t_thr_arg *)thr_args[n])->status;
    rc &= PLASMA_TEST_COND(x32 == (nthreads * iters * opval.i32));

    for (n=0; n < nthreads; ++n) {
        ((plasma_atomic_t_thr_arg *)thr_args[n])->status  =false;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->memmodel=memory_order_relaxed;
    }
    x32 = 0;
    plasma_test_nthreads(nthreads,
                         plasma_atomic_t_nthreads_add32, thr_args, NULL);
    for (n=0; n < nthreads; ++n)
        rc &= ((plasma_atomic_t_thr_arg *)thr_args[n])->status;
    rc &= PLASMA_TEST_COND(x32 == (nthreads * iters * opval.i32));


    opval.i64 = (int64_t)addval;

    for (n=0; n < nthreads; ++n) {
        ((plasma_atomic_t_thr_arg *)thr_args[n])->atomicptr=&x64;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->opvalptr=&opval.i64;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->status  =false;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->iters   =iters;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->memmodel=memory_order_seq_cst;
    }
    x64 = 0;
    plasma_test_nthreads(nthreads,
                         plasma_atomic_t_nthreads_add64, thr_args, NULL);
    for (n=0; n < nthreads; ++n)
        rc &= ((plasma_atomic_t_thr_arg *)thr_args[n])->status;
    rc &= PLASMA_TEST_COND(x64 == (nthreads * iters * opval.i64));

    for (n=0; n < nthreads; ++n) {
        ((plasma_atomic_t_thr_arg *)thr_args[n])->status  =false;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->memmodel=memory_order_acq_rel;
    }
    x64 = 0;
    plasma_test_nthreads(nthreads,
                         plasma_atomic_t_nthreads_add64, thr_args, NULL);
    for (n=0; n < nthreads; ++n)
        rc &= ((plasma_atomic_t_thr_arg *)thr_args[n])->status;
    rc &= PLASMA_TEST_COND(x64 == (nthreads * iters * opval.i64));

    for (n=0; n < nthreads; ++n) {
        ((plasma_atomic_t_thr_arg *)thr_args[n])->status  =false;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->memmodel=memory_order_relaxed;
    }
    x64 = 0;
    plasma_test_nthreads(nthreads,
                         plasma_atomic_t_nthreads_add64, thr_args, NULL);
    for (n=0; n < nthreads; ++n)
        rc &= ((plasma_atomic_t_thr_arg *)thr_args[n])->status;
    rc &= PLASMA_TEST_COND(x64 == (nthreads * iters * opval.i64));


    opval.iptr = (intptr_t)addval;

    for (n=0; n < nthreads; ++n) {
        ((plasma_atomic_t_thr_arg *)thr_args[n])->atomicptr=&xptr;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->opvalptr=&opval.iptr;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->status  =false;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->iters   =iters;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->memmodel=memory_order_seq_cst;
    }
    xptr = 0;
    plasma_test_nthreads(nthreads,
                         plasma_atomic_t_nthreads_addptr, thr_args, NULL);
    for (n=0; n < nthreads; ++n)
        rc &= ((plasma_atomic_t_thr_arg *)thr_args[n])->status;
    rc &= PLASMA_TEST_COND((intptr_t)xptr == (nthreads * iters * opval.iptr));

    for (n=0; n < nthreads; ++n) {
        ((plasma_atomic_t_thr_arg *)thr_args[n])->status  =false;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->memmodel=memory_order_acq_rel;
    }
    xptr = 0;
    plasma_test_nthreads(nthreads,
                         plasma_atomic_t_nthreads_addptr, thr_args, NULL);
    for (n=0; n < nthreads; ++n)
        rc &= ((plasma_atomic_t_thr_arg *)thr_args[n])->status;
    rc &= PLASMA_TEST_COND((intptr_t)xptr == (nthreads * iters * opval.iptr));

    for (n=0; n < nthreads; ++n) {
        ((plasma_atomic_t_thr_arg *)thr_args[n])->status  =false;
        ((plasma_atomic_t_thr_arg *)thr_args[n])->memmodel=memory_order_relaxed;
    }
    xptr = 0;
    plasma_test_nthreads(nthreads,
                         plasma_atomic_t_nthreads_addptr, thr_args, NULL);
    for (n=0; n < nthreads; ++n)
        rc &= ((plasma_atomic_t_thr_arg *)thr_args[n])->status;
    rc &= PLASMA_TEST_COND((intptr_t)xptr == (nthreads * iters * opval.iptr));


    plasma_test_free(thr_structs);
    plasma_test_free(thr_args);
    return rc;
}

int
main (int argc, char *argv[])
{
    int rc = true;
    long nprocs = plasma_sysconf_nprocessors_onln();
    if (nprocs < 1)
        nprocs = 1;
    if (nprocs > 32)
        nprocs = 32;  /*(place upper bound on num threads created in tests)*/
    /* expect all tests to complete in <2 mins  (revisit as necessary)
     * (threaded tests take more time as CPU core count increases) */
    alarm(120);

    rc &= plasma_atomic_t_relaxed();
    rc &= plasma_atomic_t_nthreads_add((int)nprocs);
    return !rc;
}
