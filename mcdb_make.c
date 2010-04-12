/* Public domain. */

typedef struct buffer {
  char *x;
  unsigned int p;
  unsigned int n;
  int fd;
  int (*op)();
} buffer;

extern void buffer_init(buffer *,int (*)(),int,char *,unsigned int);
extern int buffer_flush(buffer *);
extern int buffer_putalign(buffer *,char *,unsigned int);
extern int buffer_putflush(buffer *,char *,unsigned int);



#include "mcdb_make.h"
#include "mcdb.h"
#include "mcdb_uint32.h"

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>  /* malloc() */
#include <string.h>  /* memcpy() */

int cdb_make_start(struct cdb_make *c,int fd)
{
  c->head = 0;
  c->split = 0;
  c->hash = 0;
  c->numentries = 0;
  c->fd = fd;
  c->pos = sizeof c->final;
  buffer_init(&c->b,write,fd,c->bspace,sizeof c->bspace);
  return lseek(fd,(off_t)c->pos, SEEK_SET);
}

static int posplus(struct cdb_make *c,uint32_t len)
{
  uint32_t newpos = c->pos + len;
  if (newpos < len) { errno = ENOMEM; return -1; }
  c->pos = newpos;
  return 0;
}

int cdb_make_addend(struct cdb_make *c,unsigned int keylen,unsigned int datalen,uint32_t h)
{
  struct cdb_hplist *head;

  head = c->head;
  if (!head || (head->num >= CDB_HPLIST)) {
    head = (struct cdb_hplist *) malloc(sizeof(struct cdb_hplist));
    if (!head) return -1;
    head->num = 0;
    head->next = c->head;
    c->head = head;
  }
  head->hp[head->num].h = h;
  head->hp[head->num].p = c->pos;
  ++head->num;
  ++c->numentries;
  if (posplus(c,8) == -1) return -1;
  if (posplus(c,keylen) == -1) return -1;
  if (posplus(c,datalen) == -1) return -1;
  return 0;
}

int cdb_make_addbegin(struct cdb_make *c,unsigned int keylen,unsigned int datalen)
{
  char buf[8];

  if (keylen > 0xffffffff) { errno = ENOMEM; return -1; }
  if (datalen > 0xffffffff) { errno = ENOMEM; return -1; }

  mcdb_uint32_pack(buf,keylen);
  mcdb_uint32_pack(buf + 4,datalen);
  if (buffer_putalign(&c->b,buf,8) == -1) return -1;
  return 0;
}

int cdb_make_add(struct cdb_make *c,char *key,unsigned int keylen,char *data,unsigned int datalen)
{
  if (cdb_make_addbegin(c,keylen,datalen) == -1) return -1;
  if (buffer_putalign(&c->b,key,keylen) == -1) return -1;
  if (buffer_putalign(&c->b,data,datalen) == -1) return -1;
  return cdb_make_addend(c,keylen,datalen,mcdb_hash(key,keylen));
}

int cdb_make_finish(struct cdb_make *c)
{
  char buf[8];
  int i;
  uint32_t len;
  uint32_t u;
  uint32_t memsize;
  uint32_t count;
  uint32_t where;
  struct cdb_hplist *x;
  struct cdb_hp *hp;

  for (i = 0;i < 256;++i)
    c->count[i] = 0;

  for (x = c->head;x;x = x->next) {
    i = x->num;
    while (i--)
      ++c->count[255 & x->hp[i].h];
  }

  memsize = 1;
  for (i = 0;i < 256;++i) {
    u = c->count[i] * 2;
    if (u > memsize)
      memsize = u;
  }

  memsize += c->numentries; /* no overflow possible up to now */
  u = (uint32_t) 0 - (uint32_t) 1;
  u /= sizeof(struct cdb_hp);
  if (memsize > u) { errno = ENOMEM; return -1; }

  c->split = (struct cdb_hp *) malloc(memsize * sizeof(struct cdb_hp));
  if (!c->split) return -1;

  c->hash = c->split + c->numentries;

  u = 0;
  for (i = 0;i < 256;++i) {
    u += c->count[i]; /* bounded by numentries, so no overflow */
    c->start[i] = u;
  }

  for (x = c->head;x;x = x->next) {
    i = x->num;
    while (i--)
      c->split[--c->start[255 & x->hp[i].h]] = x->hp[i];
  }

  for (i = 0;i < 256;++i) {
    count = c->count[i];

    len = count + count; /* no overflow possible */
    mcdb_uint32_pack(c->final + 8 * i,c->pos);
    mcdb_uint32_pack(c->final + 8 * i + 4,len);

    for (u = 0;u < len;++u)
      c->hash[u].h = c->hash[u].p = 0;

    hp = c->split + c->start[i];
    for (u = 0;u < count;++u) {
      where = (hp->h >> 8) % len;
      while (c->hash[where].p)
	if (++where == len)
	  where = 0;
      c->hash[where] = *hp++;
    }

    for (u = 0;u < len;++u) {
      mcdb_uint32_pack(buf,c->hash[u].h);
      mcdb_uint32_pack(buf + 4,c->hash[u].p);
      if (buffer_putalign(&c->b,buf,8) == -1) return -1;
      if (posplus(c,8) == -1) return -1;
    }
  }

  if (buffer_flush(&c->b) == -1) return -1;
  if (lseek(c->fd,(off_t)0,SEEK_SET) == -1) return -1;
  return buffer_putflush(&c->b,c->final,sizeof c->final);
}










void buffer_init(buffer *s,int (*op)(),int fd,char *buf,unsigned int len)
{
  s->x = buf;
  s->fd = fd;
  s->op = op;
  s->p = 0;
  s->n = len;
}


static int allwrite(int (*op)(),int fd,char *buf,unsigned int len)
{
  int w;

  while (len) {
    w = op(fd,buf,len);
    if (w == -1) {
      if (errno == EINTR) continue;
      return -1; /* note that some data may have been written */
    }
    if (w == 0) ; /* luser's fault */
    buf += w;
    len -= w;
  }
  return 0;
}

int buffer_flush(buffer *s)
{
  int p;
 
  p = s->p;
  if (!p) return 0;
  s->p = 0;
  return allwrite(s->op,s->fd,s->x,p);
}

int buffer_putalign(buffer *s,char *buf,unsigned int len)
{
  unsigned int n;
 
  while (len > (n = s->n - s->p)) {
    memcpy(s->x + s->p,buf,n); s->p += n; buf += n; len -= n;
    if (buffer_flush(s) == -1) return -1;
  }
  /* now len <= s->n - s->p */
  memcpy(s->x + s->p,buf,len);
  s->p += len;
  return 0;
}

int buffer_putflush(buffer *s,char *buf,unsigned int len)
{
  if (buffer_flush(s) == -1) return -1;
  return allwrite(s->op,s->fd,buf,len);
}
