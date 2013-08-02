# NOTE: COMPLETELY UNTESTED
#
# TODO: need to compile (32-bit) mcdb.so and (64-bit) mcdb_64.so
#
#
# lam_mcdb module must be installed in /usr/lib/netsvc/dynload/mcdb.so
#
# Activation of mcdb databases:
# NSORDER environment variable overrides netsvc.conf which overrides irs.conf
#
# Example NSORDER:  (for testing before setting in system-wide config file)
#   export NSORDER=mcdb,bind
#
# Example /etc/netsvc.conf:
#   hosts=mcdb,bind
#   # Remove netsvc.conf if using irs.conf and not using aliases= in netsvc.conf
#   # ?? Does hosts=mcdb,bind behave like irs.conf 'continue' or like 'merge' ??
#
# Example /etc/irs.conf: (instead of netsvc.conf)
#   hosts mcdb continue
#   hosts bind
#   networks mcdb4
#   protocols mcdb
#   services mcdb
#   #netgroup ldap
#
#
# Aside:
# AIX does not provide a mechanism to use custom module for /etc/aliases
# (or not one documented in netsvc.conf) or else mcdb could be created for
# that, too, and netsvc.conf configured with: aliases=mcdb
# Depending on the MTA, the MTA might provide an option for using a database,
# e.g. Sendmail uses /etc/mail/aliases and builds it into aliases.db Berkeley DB
#
# -fpic ?
# 

CFLAGS+=-fpic -D_THREAD_SAFE -O3

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

mcdb.so: lam_mcdb_netdb.o
	$(CC) -o $@ -bE:lam_mcdb.exp -bnoentry $(LDFLAGS) $^ \
	  ../libnss_mcdb.a ../../libmcdb.a -lpthreads

/usr/lib/netsvc/dynload/mcdb.so: mcdb.so
	/bin/cp -f $< $@.$$$$ \
	&& /bin/mv -f $@.$$$$ $@ \
	&& /bin/chmod 555 $@

.PHONY: install clean
install: /usr/lib/netsvc/dynload/mcdb.so
clean:
	$(RM) *.o mcdb.so
