
.PHONY: all
all: mcdbctl nss_mcdbctl t/testmcdbmake t/testmcdbrand t/testzero \
     libmcdb.so libmcdb.a libnss_mcdb.a libnss_mcdb_make.a libnss_mcdb.so.2

_HAS_LIB64:=$(wildcard /lib64)
ifneq (,$(_HAS_LIB64))
  ABI_BITS=64
  ABI_FLAGS=-m64
endif

CC=gcc -pipe
CFLAGS+=$(ABI_FLAGS) -Wall -Winline -pedantic -ansi -std=c99 -D_THREAD_SAFE -O3
CFLAGS+=-D_FORTIFY_SOURCE=2 -fstack-protector
LDFLAGS+=$(ABI_FLAGS)
# To disable uint32 and nointr C99 inline functions:
#   -DNO_C99INLINE
# Another option to smaller binary is -Os instead of -O3, and remove -Winline

# heavy handed dependencies
_DEPENDENCIES_ON_ALL_HEADERS_Makefile:= $(wildcard *.h) Makefile

%.o: %.c $(_DEPENDENCIES_ON_ALL_HEADERS_Makefile)
	$(CC) -o $@ $(CFLAGS) -c $<

PIC_OBJS:= mcdb.o mcdb_make.o mcdb_makefmt.o mcdb_makefn.o \
           nointr.o uint32.o nss_mcdb.o nss_mcdb_acct.o nss_mcdb_netdb.o
$(PIC_OBJS): CFLAGS+= -fpic

# (nointr.o, uint32.o need not be included when fully inlined; adds 10K to .so)
libnss_mcdb.so.2: mcdb.o nss_mcdb.o nss_mcdb_acct.o nss_mcdb_netdb.o
	$(CC) -o $@ $(LDFLAGS) -shared -fpic \
          -Wl,-O,1 -Wl,--hash-style,gnu -Wl,-z,relro -Wl,-soname,$(@F) \
          -Wl,--version-script,nss_mcdb.map \
          $^

libmcdb.so: mcdb.o mcdb_make.o mcdb_makefmt.o mcdb_makefn.o nointr.o uint32.o
	$(CC) -o $@ $(LDFLAGS) -shared -fpic \
          -Wl,-O,1 -Wl,--hash-style,gnu -Wl,-z,relro -Wl,-soname,mcdb \
          $^

libmcdb.a: mcdb.o mcdb_error.o mcdb_make.o mcdb_makefmt.o mcdb_makefn.o \
           nointr.o uint32.o
	$(AR) -r $@ $^

libnss_mcdb.a: nss_mcdb.o nss_mcdb_acct.o nss_mcdb_netdb.o
	$(AR) -r $@ $^

libnss_mcdb_make.a: nss_mcdb_make.o nss_mcdb_acct_make.o nss_mcdb_netdb_make.o
	$(AR) -r $@ $^

mcdbctl: mcdbctl.o libmcdb.a
	$(CC) -o $@ $(CFLAGS) $^

t/%.o: CFLAGS+=-I$(CURDIR)

t/testmcdbmake: t/testmcdbmake.o libmcdb.a
	$(CC) -o $@ $(CFLAGS) $^

t/testmcdbrand: t/testmcdbrand.o libmcdb.a
	$(CC) -o $@ $(CFLAGS) $^

t/testzero: t/testzero.o libmcdb.a
	$(CC) -o $@ $(CFLAGS) $^

nss_mcdbctl: nss_mcdbctl.o libnss_mcdb_make.a libmcdb.a
	$(CC) -o $@ $(CFLAGS) $^

# (update library atomically (important to avoid crashing running programs))
# (could use /usr/bin/install if available)
/lib$(ABI_BITS)/libnss_mcdb.so.2: libnss_mcdb.so.2
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

/usr/lib$(ABI_BITS)/libnss_mcdb.so.2: /lib$(ABI_BITS)/libnss_mcdb.so.2
	[ -L $@ ] || /bin/ln -s $< $@

/usr/lib$(ABI_BITS)/libmcdb.so: libmcdb.so
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

/bin/mcdbctl: mcdbctl
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

/bin/nss_mcdbctl: nss_mcdbctl
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

.PHONY: install-headers install
install-headers: mcdb.h mcdb_error.h mcdb_make.h mcdb_makefmt.h mcdb_makefn.h \
                 code_attributes.h nointr.h uint32.h
	/bin/mkdir -p -m 0755 /usr/include/mcdb
	umask 333; /bin/cp -f --preserve=timestamps $^ /usr/include/mcdb/
install: /lib$(ABI_BITS)/libnss_mcdb.so.2 /usr/lib$(ABI_BITS)/libnss_mcdb.so.2 \
         /usr/lib$(ABI_BITS)/libmcdb.so /bin/mcdbctl /bin/nss_mcdbctl \
         install-headers


# also create 32-bit libraries for /lib on systems with /lib and /lib64
ifneq (,$(_HAS_LIB64))
ifneq (,$(ABI_BITS))
ifeq (,$(wildcard lib32))
  $(shell mkdir lib32)
endif
lib32/%.o: %.c $(_DEPENDENCIES_ON_ALL_HEADERS_Makefile)
	$(CC) -o $@ $(CFLAGS) -c $<

LIB32_PIC_OBJS:= $(addprefix lib32/,$(PIC_OBJS))
$(LIB32_PIC_OBJS): ABI_BITS=32
$(LIB32_PIC_OBJS): ABI_FLAGS=-m32
$(LIB32_PIC_OBJS): CFLAGS+= -fpic
lib32/libnss_mcdb.so.2: ABI_FLAGS=-m32
lib32/libnss_mcdb.so.2: $(addprefix lib32/, \
  mcdb.o nss_mcdb.o nss_mcdb_acct.o nss_mcdb_netdb.o)
	$(CC) -o $@ $(LDFLAGS) -shared -fpic \
          -Wl,-O,1 -Wl,--hash-style,gnu -Wl,-z,relro -Wl,-soname,$(@F) \
          -Wl,--version-script,nss_mcdb.map \
          $^
lib32/libmcdb.so: ABI_FLAGS=-m32
lib32/libmcdb.so: $(addprefix lib32/, \
  mcdb.o mcdb_make.o mcdb_makefmt.o mcdb_makefn.o nointr.o uint32.o)
	$(CC) -o $@ $(LDFLAGS) -shared -fpic \
          -Wl,-O,1 -Wl,--hash-style,gnu -Wl,-z,relro -Wl,-soname,mcdb \
          $^

/lib/libnss_mcdb.so.2: lib32/libnss_mcdb.so.2
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

/usr/lib/libnss_mcdb.so.2: /lib/libnss_mcdb.so.2
	[ -L $@ ] || /bin/ln -s $< $@

/usr/lib/libmcdb.so: lib32/libmcdb.so
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

all: lib32/libnss_mcdb.so.2

install: /lib/libnss_mcdb.so.2 /usr/lib/libnss_mcdb.so.2 /usr/lib/libmcdb.so
endif
endif


.PHONY: test test64
test64: TEST64=test64
test64: test ;
test: mcdbctl t/testzero
	$(RM) -r t/scratch
	mkdir -p t/scratch
	cd t/scratch && \
	  env - PATH="$(CURDIR):$(CURDIR)/t:$$PATH" \
	  $(CURDIR)/t/mcdbctl.t $(TEST64) 2>&1 | cat -v
	$(RM) -r t/scratch


.PHONY: clean
clean:
	! [ "$$(/usr/bin/id -u)" = "0" ]
	$(RM) *.o t/*.o
	$(RM) -r lib32
	$(RM) libmcdb.a libnss_mcdb.a libnss_mcdb_make.a
	$(RM) libmcdb.so libnss_mcdb.so.2
	$(RM) mcdbctl nss_mcdbctl t/testmcdbmake t/testmcdbrand t/testzero

