CC=gcc -pipe
CFLAGS+=-Wall -Winline -pedantic -ansi -std=c99 -D_THREAD_SAFE -O3 -g -fpic

# compile using local mcdb headers
CFLAGS+=-I ../../..

# provide path to lua.h if not in default compiler include paths
#CFLAGS+=-I /usr/include/lua5.1

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: all
all: mcdb.so mcdb_make.so

ifneq (,$(wildcard /bin/uname))
OSNAME:=$(shell /bin/uname -s)
else
OSNAME:=$(shell /usr/bin/uname -s)
endif

ifeq ($(OSNAME),Linux)
  EXTRALDFLAGS=-Wl,--version-script,luaext.map
endif
ifeq ($(OSNAME),Darwin)
  EXTRALDFLAGS=-load_hidden
endif

mcdb.so: mcdblua.o
#	$(CC) -o $@ -shared $^ -lmcdb
	$(CC) -o $@ -shared $^ $(EXTRALDFLAGS) ../../libmcdb.a

mcdb_make.so: mcdblua_make.o
#	$(CC) -o $@ -shared $^ -lmcdb
	$(CC) -o $@ -shared $^ $(EXTRALDFLAGS) ../../libmcdb.a

.PHONY: clean-mcdb clean test
test: mcdb.so mcdb_make.so
	@./mcdblua.t
	@echo "success"
clean-mcdb:
	$(RM) *.o *.so
clean: clean-mcdb
