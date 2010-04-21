/* Copyright 2010  Glue Logic LLC
 * License: GPLv3
 */

#ifndef MCDB_MAKE_H
#define MCDB_MAKE_H

#include <sys/types.h> /* size_t   */

struct mcdb_hp;                        /* (private structure) */

struct mcdb_make {
  size_t pos;
  size_t fsz;
  char * restrict map;
  struct mcdb_hplist *head;
  int fd;
  void * (*fn_malloc)(size_t);         /* fn ptr to malloc() */
  void (*fn_free)(void *);             /* fn ptr to free() */
};

extern int
mcdb_make_start(struct mcdb_make * restrict, int,
                void * (*)(size_t), void (*)(void *));
extern int
mcdb_make_add(struct mcdb_make * restrict,
              const char * restrict, size_t,
              const char * restrict, size_t);
extern int
mcdb_make_finish(struct mcdb_make * restrict);
extern int
mcdb_make_destroy(struct mcdb_make * restrict);

/* support for adding entries from input stream, instead of fully in memory */
extern int
mcdb_make_addbegin(struct mcdb_make * restrict, size_t, size_t);
extern void
mcdb_make_addbuf_key(struct mcdb_make * restrict,const char * restrict,size_t);
extern void
mcdb_make_addbuf_data(struct mcdb_make * restrict,const char * restrict,size_t);
extern void
mcdb_make_addend(struct mcdb_make * restrict);
extern void
mcdb_make_addrevert(struct mcdb_make * restrict);

#endif
