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

#ifndef INCLUDED_PLASMA_SYSCONF_H
#define INCLUDED_PLASMA_SYSCONF_H

#include "plasma_attr.h"    
PLASMA_ATTR_Pragma_once

#ifdef __cplusplus
extern "C" {
#endif


long
plasma_sysconf_nprocessors_onln (void);

long
plasma_sysconf_pagesize (void);


#ifdef __cplusplus
}
#endif

#endif
