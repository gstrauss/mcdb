/*
 * nss_mcdbctl - command line tool to create mcdb of nsswitch.conf databases
 *
 * Copyright (c) 2018, Glue Logic LLC. All rights reserved. code()gluelogic.com
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
#define _XOPEN_SOURCE 600
#endif

#include "nss_mcdb_netdb.h"

#include <nss.h>   /* NSS_STATUS_{TRYAGAIN,UNAVAIL,NOTFOUND,SUCCESS,RETURN} */

int main (int argc, char *argv[])
{
    /*(should use getopt())*/
    const char *h = "", *u = "", *d = "";
    char *netgroup = NULL;
    if (argc < 2) return 2;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (i+1 == argc) return 2;
            switch (argv[i++][1]) {
              case 'h': h = argv[i]; break;
              case 'u': u = argv[i]; break;
              case 'd': d = argv[i]; break;
              default:  return 2;
            }
            continue;
        }

        if (NULL == netgroup)
            netgroup = argv[i];
        else
            return 2;
    }
    if (NULL == netgroup)
        return 2;

    char buf[1024];
    int errnum;
    switch (_nss_mcdb_innetgr(netgroup,h,u,d,buf,sizeof(buf),&errnum)) {
      case NSS_STATUS_SUCCESS:  return 0;
      case NSS_STATUS_NOTFOUND: return 1;
      default:                  return 2;
      case NSS_STATUS_TRYAGAIN:
      case NSS_STATUS_UNAVAIL:  return 3;
    }
}
