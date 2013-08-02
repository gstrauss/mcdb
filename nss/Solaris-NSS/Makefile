# NOTE: COMPLETELY UNTESTED
#
# TODO: need to compile 32-bit and 64-bit nss_mcdb.so.1
# TODO: create an exports map file to limit symbols exposed by .so
# TODO: nss_mcdb.so.1 must be installed in /lib and lib/sparcv9 (?)
#
# Activation of mcdb databases is handled in /etc/nsswitch.conf similar to Linux
#
# -fpic ?
# 

CFLAGS+=-fpic -D_THREAD_SAFE -O3

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

nss_mcdb.so.1: nss_mcdb_solaris.o
	$(CC) -o $@ -shared -fpic $(LDFLAGS) $^ \
	  ../libnss_mcdb.a ../../libmcdb.a -lsocket -lnsl

.PHONY: clean
clean:
	$(RM) *.o nss_mcdb.so.1
