
.PHONY: all
all: mcdbctl nss_mcdbctl testzero \
     libmcdb.a libnss_mcdb.a libnss_mcdb_make.a libnss_mcdb.so.2

CC=gcc
CFLAGS+=-pipe -Wall -Winline -pedantic -ansi -std=c99 -D_THREAD_SAFE -O3
# To disable uint32 and nointr C99 inline functions:
#   -DNO_C99INLINE
# Another option to smaller binary is -Os instead of -O3, and remove -Winline

# heavy handed dependencies
_DEPENDENCIES_ON_ALL_HEADERS_Makefile:= $(wildcard *.h) Makefile

%.o: %.c $(_DEPENDENCIES_ON_ALL_HEADERS_Makefile)
	$(CC) $(CFLAGS) -c -o $@ $<

# (nointr.o, uint32.o need not be included when fully inlined; adds 10K to .so)
PIC_OBJS:= mcdb.o nss_mcdb.o nss_mcdb_acct.o nss_mcdb_netdb.o
$(PIC_OBJS): CFLAGS+= -fpic
libnss_mcdb.so.2: $(PIC_OBJS)
	$(CC) -shared -fpic -Wl,-soname,$(@F) -o $@ $^

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
/lib/libnss_mcdb.so.2: libnss_mcdb.so.2
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

/usr/lib/libnss_mcdb.so.2: /lib/libnss_mcdb.so.2
	[ -L $@ ] || /bin/ln -s $< $@

/bin/mcdbctl: mcdbctl
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

/bin/nss_mcdbctl: nss_mcdbctl
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

.PHONY: install
install: /lib/libnss_mcdb.so.2 /usr/lib/libnss_mcdb.so.2 \
         /bin/mcdbctl /bin/nss_mcdbctl


# 64-bit nss_mcdb_* library
# (hack: most modern CPUs in high-end computers are 64-bit; though not i686)
ifneq (i686,$(shell uname -p))
ifeq (,$(wildcard lib64))
  $(shell mkdir lib64)
endif
lib64/%.o: %.c $(_DEPENDENCIES_ON_ALL_HEADERS_Makefile)
	$(CC) $(CFLAGS) -c -o $@ $<

LIB64_PIC_OBJS:= $(addprefix lib64/,$(PIC_OBJS))
$(LIB64_PIC_OBJS): CFLAGS+= -fpic -m64
lib64/libnss_mcdb.so.2: $(LIB64_PIC_OBJS)
	$(CC) -shared -fpic -Wl,-soname,$(@F) -o $@ $^

/lib64/libnss_mcdb.so.2: lib64/libnss_mcdb.so.2
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@

/usr/lib64/libnss_mcdb.so.2: /lib64/libnss_mcdb.so.2
	[ -L $@ ] || /bin/ln -s $< $@

all: lib64/libnss_mcdb.so.2

install: /lib64/libnss_mcdb.so.2 /usr/lib64/libnss_mcdb.so.2
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
	$(RM) -r lib64
	$(RM) mcdbctl nss_mcdbctl testzero

