/* Public domain. */

#ifndef CDB_MAKE_H
#define CDB_MAKE_H

#include <stdint.h>

#define CDB_HPLIST 1000

struct cdb_hp { uint32_t h; uint32_t p; } ;

struct cdb_hplist {
  struct cdb_hp hp[CDB_HPLIST];
  struct cdb_hplist *next;
  int num;
} ;

struct cdb_make {
  char bspace[8192];
  char final[2048];
  uint32_t count[256];
  uint32_t start[256];
  struct cdb_hplist *head;
  struct cdb_hp *split; /* includes space for hash */
  struct cdb_hp *hash;
  uint32_t numentries;
  uint32_t pos;
  int fd;
  buffer b;
} ;

extern int cdb_make_start(struct cdb_make *,int);
extern int cdb_make_addbegin(struct cdb_make *,unsigned int,unsigned int);
extern int cdb_make_addend(struct cdb_make *,unsigned int,unsigned int,uint32_t);
extern int cdb_make_add(struct cdb_make *,char *,unsigned int,char *,unsigned int);
extern int cdb_make_finish(struct cdb_make *);

#endif
