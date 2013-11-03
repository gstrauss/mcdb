/*
 * plasma_sysconf - system configuration info
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

/* _AIX requires minimum standards level for _SC_PAGESIZE define */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#define PLASMA_FEATURE_DISABLE_WIN32_FULLHDRS

#include "plasma_sysconf.h"
#include "plasma_feature.h"

#ifdef PLASMA_FEATURE_POSIX
#include <unistd.h>
/* FUTURE: might make interfaces into macros which simply call sysconf() */
#elif defined(_WIN32)
#include <windows.h>
/* SYSTEM_INFO structure
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms724958%28v=vs.85%29.aspx
 * GetLogicalProcessorInformation (contains more complex code example)
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms683194%28v=vs.85%29.aspx */
#endif

/* plasma_sysconf_nprocessors_onln()
 * sysconf _SC_NPROCESSORS_ONLN and _SC_NPROCESSORS_CONF are not required by
 * standards, but are provided on numerous unix platforms and documented as
 * optional by Open Group.
 * http://www.opengroup.org/austin/docs/austin_512.txt
 * http://austingroupbugs.net/view.php?id=339
 * http://www.mail-archive.com/tech@openbsd.org/msg01435.html
 * Implemented by AIX/Tru64/Solaris/Linux/FreeBSD/NetBSD/OpenBSD/Darwin.
 * https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/sysconf.3.html
 * http://stackoverflow.com/questions/4586405/get-number-of-cpus-in-linux-using-c
 */

#ifdef _AIX  /*(AIX requires _ALL_SOURCE defined for this definition)*/
#define _SC_NPROCESSORS_ONLN            72
#endif

long
plasma_sysconf_nprocessors_onln (void)
{
    long nprocs_onln;

    #ifdef _SC_NPROCESSORS_ONLN
    nprocs_onln = sysconf(_SC_NPROCESSORS_ONLN);
    #elif defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    nprocs_onln = info.dwNumberOfProcessors;
    #else
    #error "sysconf(_SC_NPROCESSORS_ONLN) not implemented"
    #endif

    return nprocs_onln;
}

long
plasma_sysconf_pagesize (void)
{
    long pagesize;
    #ifdef _SC_PAGESIZE
    pagesize = sysconf(_SC_PAGESIZE);
    #elif defined(_WIN32)
      /* http://msdn.microsoft.com/en-us/library/windows/desktop/cc644950%28v=vs.85%29.aspx
       * Page size is 4,096 bytes on x64 and x86 or 8,192 bytes for Itanium-based systems.
       */
      #if defined(_M_AMD64) || defined(_M_IX86)
        pagesize = 4096;
      #elif defined(_M_IA64)
        pagesize = 8192;
      #else
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        pagesize = info.dwPageSize;
      #endif
    #else
    #error "sysconf(_SC_PAGESIZE) not implemented"
    #endif

    return pagesize;
}
