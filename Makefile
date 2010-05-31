
.PHONY: all other_objects
all: mcdbctl testzero nss libmcdb.a libnss_mcdb.so.2

CC=gcc
CFLAGS+=-pipe -Wall -Winline -pedantic -ansi -std=c99 -D_THREAD_SAFE \
        -fpic -O3 -g -I.

# heavy handed dependencies
_DEPENDENCIES_ON_ALL_HEADERS_Makefile:= $(wildcard *.h) Makefile

%.o: %.c $(_DEPENDENCIES_ON_ALL_HEADERS_Makefile)
	$(CC) $(CFLAGS) -c -o $@ $<

libmcdb.a: mcdb.o mcdb_error.o mcdb_make.o mcdb_makefmt.o nointr.o uint32.o
	$(AR) -r $@ $^

# (nointr.o, uint32.o need not be included when fully inlined; adds 10K to .so)
libnss_mcdb.so.2: mcdb.o \
                  nss_mcdb.o nss_mcdb_acct.o nss_mcdb_misc.o nss_mcdb_netdb.o
	$(CC) -shared -fpic -Wl,-soname,$(@F) -o $@ $^

mcdbctl:  mcdbctl.o libmcdb.a
	$(CC) -o $@ $(CFLAGS) $^
testzero: testzero.o libmcdb.a
	$(CC) -o $@ $(CFLAGS) $^

.PHONY: clean
clean:
	$(RM) *.o libmcdb.a mcdbctl testzero

.PHONY: nss
nss: nss_mcdb.o nss_mcdb_acct.o nss_mcdb_misc.o nss_mcdb_netdb.o \
     nss_mcdb_acct_make.o nss_mcdb_misc_make.o nss_mcdb_netdb_make.o
