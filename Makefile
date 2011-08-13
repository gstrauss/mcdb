
.PHONY: all
all: mcdbctl nss_mcdbctl testzero \
     libmcdb.a libnss_mcdb.a libnss_mcdb_make.a libnss_mcdb.so.2

_HAS_LIB64:=$(wildcard /lib64)
ifneq (,$(_HAS_LIB64))
  ABI_BITS=64
  ABI_FLAGS=-m64
endif

CC=gcc -pipe
CFLAGS+=$(ABI_FLAGS) -Wall -Winline -pedantic -ansi -std=c99 -D_THREAD_SAFE -O3
LDFLAGS+=$(ABI_FLAGS)
# To disable uint32 and nointr C99 inline functions:
#   -DNO_C99INLINE
# Another option to smaller binary is -Os instead of -O3, and remove -Winline

# heavy handed dependencies
_DEPENDENCIES_ON_ALL_HEADERS_Makefile:= $(wildcard *.h) Makefile

%.o: %.c $(_DEPENDENCIES_ON_ALL_HEADERS_Makefile)
	$(CC) -o $@ $(CFLAGS) -c $<

# (nointr.o, uint32.o need not be included when fully inlined; adds 10K to .so)
PIC_OBJS:= mcdb.o nss_mcdb.o nss_mcdb_acct.o nss_mcdb_netdb.o
$(PIC_OBJS): CFLAGS+= -fpic
libnss_mcdb.so.2: $(PIC_OBJS)
	$(CC) -o $@ $(LDFLAGS) -shared -fpic -Wl,-soname,$(@F) $^

libmcdb.a: mcdb.o mcdb_error.o mcdb_make.o mcdb_makefmt.o nointr.o uint32.o
	$(AR) -r $@ $^

libnss_mcdb.a: nss_mcdb.o nss_mcdb_acct.o nss_mcdb_netdb.o
	$(AR) -r $@ $^

libnss_mcdb_make.a: nss_mcdb_make.o nss_mcdb_acct_make.o nss_mcdb_netdb_make.o
	$(AR) -r $@ $^

mcdbctl: mcdbctl.o libmcdb.a
	$(CC) -o $@ $(CFLAGS) $^

testzero: testzero.o libmcdb.a
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

/bin/mcdbctl: mcdbctl
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

/bin/nss_mcdbctl: nss_mcdbctl
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

.PHONY: install
install: /lib$(ABI_BITS)/libnss_mcdb.so.2 /usr/lib$(ABI_BITS)/libnss_mcdb.so.2 \
         /bin/mcdbctl /bin/nss_mcdbctl


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
lib32/libnss_mcdb.so.2: $(LIB32_PIC_OBJS)
	$(CC) -o $@ $(LDFLAGS) -shared -fpic -Wl,-soname,$(@F) $^

/lib/libnss_mcdb.so.2: lib32/libnss_mcdb.so.2
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

/usr/lib/libnss_mcdb.so.2: /lib/libnss_mcdb.so.2
	[ -L $@ ] || /bin/ln -s $< $@

all: lib32/libnss_mcdb.so.2

install: /lib/libnss_mcdb.so.2 /usr/lib/libnss_mcdb.so.2
endif
endif


.PHONY: test
test: mcdbctl
	$(RM) -r t/scratch
	mkdir -p t/scratch
	cd t/scratch && \
	  env - PATH="$(CURDIR):$$PATH" \
	  $(CURDIR)/t/mcdbctl.t 2>&1 | cat -v
	$(RM) -r t/scratch


.PHONY: clean
clean:
	! [ "$$(/usr/bin/id -u)" = "0" ]
	$(RM) *.o libmcdb.a libnss_mcdb.a libnss_mcdb_make.a libnss_mcdb.so.2
	$(RM) -r lib32
	$(RM) mcdbctl nss_mcdbctl testzero

