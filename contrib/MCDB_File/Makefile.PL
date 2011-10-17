use 5.005;
use strict;
use warnings;

use Config;
use ExtUtils::MakeMaker;

WriteMakefile(
    'NAME'                 => 'MCDB_File',
    'AUTHOR'               => 'gstrauss  <code () gluelogic.com>',
    'VERSION_FROM'         => 'MCDB_File.pm',
    'ABSTRACT_FROM'        => 'MCDB_File.pm',
    'PL_FILES'             => {},
    ($ExtUtils::MakeMaker::VERSION < 6.3002 ? () :
      ('LICENSE'           => 'perl', )),
    PREREQ_PM              => { 'Test::More' => 0, },  # For testing
    dist                   => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean                  => { FILES => 'MCDB_File-*' },
    # compile and link with local, static libmcdb.a
    INC         => '-I ../../..',
    dynamic_lib => {
      OTHERLDFLAGS => '../../libmcdb.a -Wl,--version-script,perlext.map',
    },
    # compile and link with system libmcdb.so
    #LIBS       => ['-lmcdb'],
    NEEDS_LINKING          => 1,
    ($ExtUtils::MakeMaker::VERSION < 6.46 ? () :
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
