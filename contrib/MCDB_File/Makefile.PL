use 5.005;
use strict;
use warnings;

use version 0.77;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME                   => 'MCDB_File',
    AUTHOR                 => 'gstrauss  <code () gluelogic.com>',
    VERSION_FROM           => 'MCDB_File.pm',
    ABSTRACT_FROM          => 'MCDB_File.pm',
    PL_FILES               => {},
    (version->parse($ExtUtils::MakeMaker::VERSION) < 6.3002 ? () :
      (LICENSE             => 'perl', )),
    PREREQ_PM              => { 'Test::More' => 0, },  # For testing
    dist                   => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean                  => { FILES => 'MCDB_File-*' },
    # compile and link with local, static libmcdb.a
    INC                    => '-I .',    # include <mcdb/mcdb*.h>
    MYEXTLIB               => 'mcdb/libmcdb.a',
    dynamic_lib => {
      OTHERLDFLAGS =>
        (  $^O eq 'linux'   ? ' -Wl,--version-script,perlext.map'
         : $^O eq 'solaris' ? ' -lrt'
         :                    ''),
    },
    # compile and link with system libmcdb.so
    #LIBS       => ['-lmcdb'],
    NEEDS_LINKING          => 1,
    (version->parse($ExtUtils::MakeMaker::VERSION) < 6.46 ? () :
      (META_MERGE => {
            recommends     => { },
            build_requires => { 'Test::More' => 0, },  # For testing
            resources      => {
                license => 'http://dev.perl.org/licenses/',
                homepage => 'https://github.com/gstrauss/MCDB_File',
                repository => 'https://github.com/gstrauss/MCDB_File',
            },
      })),
);

sub MY::postamble {
    return <<'MAKE_FRAG';
mcdb: mcdb.tar.gz
	gunzip -c $< | tar xf -

mcdb/libmcdb.a: mcdb
	$(MAKE) -C mcdb libmcdb.a

clean::
	[ ! -d mcdb ] || $(MAKE) -C mcdb clean

realclean::
	-$(RM) -r mcdb
MAKE_FRAG
}
