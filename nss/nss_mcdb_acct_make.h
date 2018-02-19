/*
 * nss_mcdb_acct_make - mcdb of passwd, group nsswitch.conf databases
 *
 * Copyright (c) 2010, Glue Logic LLC. All rights reserved. code()gluelogic.com
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

#ifndef INCLUDED_NSS_MCDB_ACCT_MAKE_H
#define INCLUDED_NSS_MCDB_ACCT_MAKE_H

#include "../plasma/plasma_feature.h"
#include "../plasma/plasma_attr.h"
#include "../plasma/plasma_stdtypes.h"
#include "nss_mcdb_make.h"
PLASMA_ATTR_Pragma_once

#include <pwd.h>
#include <grp.h>

/* buf size passed should be _SC_GETPW_R_SIZE_MAX + NSS_PW_HDRSZ (not checked)*/
__attribute_nonnull__
size_t
nss_mcdb_acct_make_passwd_datastr(char * restrict, const size_t,
			          const struct passwd * const restrict);

/* buf size passed should be _SC_GETGR_R_SIZE_MAX + NSS_GR_HDRSZ (not checked)*/
__attribute_nonnull__
size_t
nss_mcdb_acct_make_group_datastr(char * restrict, const size_t,
			         const struct group * const restrict);


/*
 * Note: mcdb *_make_* routines are not thread-safe
 * (no need for thread-safety; mcdb is typically created from a single stream)
 *
 * nss_mcdb_acct_make_group_encode() is not reentrant due to private static vars
 * (do not interleave calls encoding two or more 'group' databases at same time)
 *
 * #define NSS_MCDB_NGROUPS_MAX is set in nss_mcdb_acct.h to a reasonable value
 * used in sizing some data structures.  /etc/group with many members may exceed
 * the limit imposed here, but should be plenty for average standalone systems.
 * Recompile with #define value in header set to a large size, if needed.
 *
 * nss_mcdb_acct*.[ch] code expects 1:1 mapping between uid/user and gid/group.
 * Validate file-based databases (e.g. /etc/passwd and /etc/group) after the
 * files have been modified and before renaming into place (e.g. vipw, vigr).
 * Some vendors provide validation tools (e.g. pwck, grpck) to report multiple
 * lines with same user name or group name, and duplication of uid or gid.
 * (If these are not expected on your system, be sure to validate and detect.)
 * Duplication of a username in group memberships is mostly harmless, other than
 * taking up space in the grouplist, but should be detected and corrected before
 * nss_mcdb_* code is called.  (future: add validation to nss_mcdb_*_make.[ch])
 *
 * Historical line length limitations in passwd and group (and shadow) databases
 * (e.g. 512 bytes on Solaris 8; < 1024 bytes in dbm(3x) routines used by NIS;
 *  older versions of glibc, number of group members limited to 200 and lines
 *  longer than 1024 chars were silently skipped) led to solutions such as
 * listing a gid on multiple lines in /etc/group, with additional members on
 * each line, for supplemental groups that contained many members.  This is
 * NOT supported by nss_mcdb_acct_make.[ch] and will not be detected -- the
 * grouplist returned for the group will be that of only the first line.
 * Modern operating systems have removed this arbitrary limit.  A slight
 * alteration to listing group and gid on multiple lines is to have multiple
 * groups with the same gid, but with different names.  This, too, is NOT
 * supported by nss_mcdb_acct_make.[ch] and will not be detected -- getgrgid
 * will return struct group based on first line containing the gid, and will not
 * include subsequent lines containing gid.  (This is consistent with behavior
 * of how most (all?) modern systems scan /etc/group.)  Similar is true for
 * /etc/passwd containing multiple lines with different names, but same uid.
 * getpwuid will return struct passwd based on first line containing the uid.
 *
 * Nested groups (groups as members of other groups) is NOT supported in
 * standard unix systems.  Recommended workaround is to preprocess a data file
 * to expand well-defined structures containing nested groups into a flattened
 * /etc/group.  If this is done, take care to ensure that nothing other than the
 * preprocessor modifies /etc/group, and that the resultant /etc/group not have
 * undesirable duplication, excessively long lines, or excessive number of group
 * members (that exceeds NSS_MCDB_NGROUPS_MAX).
 *
 * Aside: user membership of many groups while using NFSv2,NFSv3 is not portable
 * past 16 groups on modern unix systems.  This is due to a limitation in the
 * default NFS RPC credentials passing mechanism (client to server) of AUTH_SYS,
 * defined in RFC 1050 (RPC: Remote Procedure Call Protocol specification) in
 * 1988 with a struct definition holding 10 gids, and superceded by RFC 1057
 * (RPC: Remote Procedure Call Protocol Specification Version 2) in 1998 and
 * RFC 1831 (RPC: Remote Procedure Call Protocol Specification Version 2) in
 * 1995 with the struct definition holding 16 gids.  Although Linux suppports
 * users with more than 16 group memberships -- previously 32 groups, and now
 * 65536 in Linux kernel 2.6 -- Linux will silently truncate the group
 * membership list to 16 when sending RPC AUTH_SYS credentials to an NFS server.
 * Options to support additional groups include:
 * - rpc.mountd -g (or --manage-gids; not portable; available on Linux variants)
 * - NFSv4 with RPCSEC_GSS auth, kerberos authentication
 * - NFSv3 with RPCSEC_GSS auth (instead of RPC AUTH_SYS)
 * - ACLs
 * NFSv4 with kerberos authentication (not yet supported on all unix) or
 * NFSv3,NFSv4 with rpc.mountd -g (not portable) move the credential lookup to
 * server side, rather than having client pass the grouplist via RPC AUTH_SYS.
 * Using RPCSEC_GSS instead of AUTH_SYS with NFSv3 works for more than 16
 * supplemental group memberships EXCEPT that NLM byte-range locking is still
 * limited to 16 groups -- i.e. do not use with more than 16 groups if fcntl
 * locking should have a chance of working.
 * Using ACLs leaves behind traditional unix permission model (user/group/other)
 * (On Linux, ACLs can be implemented with DCE/DFS implementation such as AFS.
 *  For portability, AFS should not be used in conjunction with more than 13
 *  unix supplement traditional groups.)  Using ACLs can lead quickly to
 * complexity and make permissions more opaque as simple tools like 'ls -l' will
 * no longer show the whole picture.  Also, backup software needs to be able to
 * recognize, backup, and restore ACLs.  Other traditional unix tools that work
 * with the traditional unix permission model might not understand ACLs.  Those
 * caveats acknowledged, ACLs are an alternative solution to traditional
 * supplemental group membership limits.  For simplicity and maintainability,
 * it is recommended that when using ACLs, ACLs be applied sparingly or applied
 * consistently upon whole trees to organize files, instead of many exceptions.
 * References:
 *http://blogs.sun.com/peteh/entry/increasing_unix_group_membership_easy
 *http://nfsworld.blogspot.com/2005/03/whats-deal-on-16-group-id-limitation.html
 *http://sadiquepp.blogspot.com/2009/02/how-to-configure-nfsv4-with-kerberos-in.html
 *http://www.openafs.org/
 */



__attribute_nonnull__
bool
nss_mcdb_acct_make_passwd_encode(
  struct nss_mcdb_make_winfo * const restrict,
  const void * const);

__attribute_nonnull__
bool
nss_mcdb_acct_make_group_encode(
  struct nss_mcdb_make_winfo * const restrict,
  const void * const);


__attribute_nonnull__
bool
nss_mcdb_acct_make_group_flush(
  struct nss_mcdb_make_winfo * const restrict);


__attribute_nonnull__
bool
nss_mcdb_acct_make_passwd_parse(
  struct nss_mcdb_make_winfo * const restrict, char * restrict, size_t);

__attribute_nonnull__
bool
nss_mcdb_acct_make_group_parse(
  struct nss_mcdb_make_winfo * const restrict, char * restrict, size_t);


#endif
