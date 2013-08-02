/*
 * lam_mcdb_acct - query mcdb of passwd, group
 *
 * Copyright (c) 2011, Glue Logic LLC. All rights reserved. code()gluelogic.com
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
 * NOT IMPLEMENTED; research notes follow
 */

/*
 * AIX-specific code for AIX LAM
 *
 * (AIX LAM == AIX Loadable Authentication Module Programming Interface)
 * http://publib.boulder.ibm.com/infocenter/aix/v6r1/index.jsp?topic=%2Fcom.ibm.aix.kernelext%2Fdoc%2Fkernextc%2Fsec_load_mod.htm
 *
 * Loadable modules can provide identification, authentication, or both.
 * Given that AIX supports PAM, a PAM modules is probably more advisable
 * if not using something more centralized like LDAP.
 *
 * The following are notes about methods needed to support identification.
 * The methods are not well-documented at level of detail of a basic 'man' page
 * with information required to implement the interface without guess-and-check.
 * (at least not at the URL above)
 *
 *   method_open
 *     void *method_open (char *name, char *domain, int mode, char *options);
 *       structure allocated with state and passed as handlep to calls below
 *       (malloc() space for db buffers and structures, including struct mcdb)
 *       (Are the AIX interfaces thread-safe or do locks need to be taken
 *        when using the void *handlep?  Who provides storage into which to
 *        write results; which routines expect to free() storage from malloc()?)
 *   method_close
 *     void method_close (void *handlep);
 *
 * (interfaces required for identification module)
 *   method_getpwnam
 *     struct passwd *method_getpwnam (void *handlep, const char *user);
 *       (_nss_mcdb_getpwnam_r())
 *   method_getpwuid
 *     struct passwd *method_getpwuid (void *handlep, uid_t uid);
 *       (_nss_mcdb_getpwuid_r())
 *   method_getgrgid
 *     struct group *method_getgrgid (void *handlep, gid_t gid);
 *       (_nss_mcdb_getgrgid_r())
 *   method_getgrnam
 *     struct group *method_getgrnam (void *handlep, const char *group);
 *       (_nss_mcdb_getgrnam_r())
 *   method_getgrset
 *     char *method_getgrset (void *handlep, char *user);
 *       (nss_mcdb_getgrouplist(), and then translate into names)
 *         (we do not have initial gid_t, so pass (gid_t)-1 and later filter it)
 *       (AIX probably redundantly turns this string list back into gid_t list)
 *       malloc() memory for list of comma-separate group names returned
 *       (caller of getgrset() must free() it)
 * 
 * (interfaces not required, but probably not too difficult to implement)
 *   method_getgracct
 *     struct group *method_getgracct (void *handlep, void *id, int type);
 *       (_nss_mcdb_getgrgid_r())
 *       (_nss_mcdb_getgrnam_r())
 *   method_getgrusers
 *     int method_getgrusers (void *handlep, char *group,
 *                            void *result, int type, int *size);
 *       (_nss_mcdb_getgrgid_r())
 *       (_nss_mcdb_getgrnam_r())
 */
