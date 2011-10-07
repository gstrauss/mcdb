CC=gcc -pipe
CFLAGS+=-Wall -Winline -pedantic -ansi -std=c99 -D_THREAD_SAFE -O3 -g

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: all
all: mcdb.so mcdb_make.so

mcdb.so: mcdblua.o
	$(CC) -o $@ -shared $^ -lmcdb

mcdb_make.so: mcdblua_make.o
	$(CC) -o $@ -shared $^ -lmcdb

.PHONY: clean-mcdb clean test
test: mcdb.so mcdb_make.so
	@./mcdblua.t
	@echo "success"
clean-mcdb:
	$(RM) *.o *.so
clean: clean-mcdb
