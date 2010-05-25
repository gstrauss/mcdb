
all: mcdbctl testzero nss

CC=gcc
CFLAGS+=-pipe -Wall -pedantic -ansi -std=c99 -D_THREAD_SAFE -O3 -g -I.

# heavy handed dependencies
_DEPENDENCIES_ON_ALL_HEADERS_Makefile:= $(wildcard *.h) Makefile

%.o: %.c $(_DEPENDENCIES_ON_ALL_HEADERS_Makefile)
	$(CC) $(CFLAGS) -c -o $@ $<

mcdbctl:  mcdb.o mcdb_makefmt.o mcdb_make.o mcdb_error.o mcdbctl.o
	$(CC) -o $@ $(CFLAGS) $^
testzero: mcdb.o mcdb_makefmt.o mcdb_make.o mcdb_error.o testzero.o
	$(CC) -o $@ $(CFLAGS) $^

.PHONY: all clean
clean:
	$(RM) *.o mcdbctl testzero

.PHONY: nss
nss: nss_mcdb.o nss_mcdb_acct.o nss_mcdb_misc.o nss_mcdb_netdb.o \
     nss_mcdb_acct_make.o nss_mcdb_misc_make.o nss_mcdb_netdb_make.o
