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

#include "plasma_spin.h"

#ifndef plasma_spin_lock_acquire_spinloop
bool
plasma_spin_lock_acquire_spinloop (plasma_spin_lock_t * const spin)
{
    uint32_t * const lck = &spin->lck;
    do {
        while (*(volatile uint32_t *)lck) {
            plasma_spin_pause();
        }
    } while (!plasma_atomic_lock_acquire(lck)); /*(includes barrier)*/
    return true;
}
#endif

bool
plasma_spin_lock_acquire_spindecay (plasma_spin_lock_t * const spin,
                                    int pause, int pause32, int yield)
{
    /* ((uint32_t *) cast also works for Apple OSSpinLock, which is int32_t) */
    uint32_t * const lck = (uint32_t *)&spin->lck;
    do {
        while (*(volatile uint32_t *)lck) {
            if (pause) {
                --pause;
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
