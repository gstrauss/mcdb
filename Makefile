
all: mcdbctl testzero

CC=gcc
CFLAGS+=-pipe -Wall -pedantic -ansi -std=c99 -D_THREAD_SAFE -O3 -g -I.

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

mcdbctl:  mcdb.o mcdb_makefmt.o mcdb_make.o mcdb_error.o mcdbctl.o
	$(CC) -o $@ $(CFLAGS) $^
testzero: mcdb.o mcdb_makefmt.o mcdb_make.o mcdb_error.o testzero.o
	$(CC) -o $@ $(CFLAGS) $^

.PHONY: all clean
clean:
	$(RM) *.o mcdbctl testzero
