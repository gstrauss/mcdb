use 5.005;
use strict;
use warnings;

use Config;
use ExtUtils::MakeMaker;

# (version.pm not portable until Perl 5.9 (?); Perl 5.10.0 uses _ in versions)
my $ExtUtils_MakeMaker_VERSION = $ExtUtils::MakeMaker::VERSION;
$ExtUtils_MakeMaker_VERSION =~ tr/_//d;

WriteMakefile(
    NAME                   => 'MCDB_File',
    AUTHOR                 => 'gstrauss  <code () gluelogic.com>',
    VERSION_FROM           => 'MCDB_File.pm',
    ABSTRACT_FROM          => 'MCDB_File.pm',
    PL_FILES               => {},
    ($ExtUtils_MakeMaker_VERSION < 6.3002 ? () :
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
         : $^O eq 'darwin'  ? ' -load_hidden'
         : $^O eq 'solaris' ? ' -lrt'
         :                    ''),
    },
    # compile and link with system libmcdb.so
    #LIBS       => ['-lmcdb'],
    NEEDS_LINKING          => 1,
    ($ExtUtils_MakeMaker_VERSION < 6.46 ? () :
      (META_MERGE => {
            recommends     => { },
            build_requires => { 'Test::More' => 0, },  # For testing
            resources      => {
                license => 'http://dev.perl.org/licenses/',
                homepage => 'https://github.com/gstrauss/mcdb',
                repository => 'https://github.com/gstrauss/mcdb',
            },
      })),
);

sub MY::postamble {
    # Building mcdb requires C99 but different compilers require different flags
    # so at the moment, rules below build mcdb with gcc (which must be present)
    # XXX: Future: pass to mcdb gmake compiler-specific flags to enable C99 and:
    #        CC='$(CC)' RPM_OPT_FLAGS='$(CCFLAGS)' ABI_FLAGS= MCDB_SKIP32= \
    #           ABI_BITS=($Config{ptrsize} == 8 ? 64 : 32) # evaluated
    my $postamble =
      "mcdb/libmcdb.a: ABI_BITS=".($Config{ptrsize} == 8 ? 64 : 32)."\n\n";
    $postamble .= <<'MAKE_FRAG';
mcdb/libmcdb.a: mcdb
	$(MAKE) -C mcdb libmcdb.a ABI_BITS=$(ABI_BITS)

mcdb: mcdb.tar.gz
	gunzip -c $< | tar xf -

clean::
	[ ! -d mcdb ] || $(MAKE) -C mcdb clean

realclean::
	-$(RM) -r mcdb
MAKE_FRAG
    return $postamble;
}
