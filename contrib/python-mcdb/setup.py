#! /usr/bin/env python

SRCDIR   = "src"
SRCFILES = map(lambda f: SRCDIR + '/' + f + '.c', ["mcdbpy"])

from distutils.core import setup, Extension

setup (# Distribution meta-data
        name = "python-mcdb",
        version = "0.01",
        description = "Interface to mcdb constant database files",
        author = "gstrauss",
        author_email = "code()gluelogic.com",
	license = "GPL",
        long_description = \
'''Python interface to create and read mcdb constant databases.

The python-mcdb extension module wraps interfaces in libmcdb.so
See https://github.com/gstrauss/mcdb/ for latest info on mcdb.
mcdb is based on D. J. Bernstein's constant database package
(see http://cr.yp.to/cdb.html).''',
        ext_modules = [ Extension( "mcdb", SRCFILES, libraries=['mcdb']) ],
        url = "https://github.com/gstrauss/mcdb/",
      )

