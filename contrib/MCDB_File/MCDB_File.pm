package MCDB_File;

use strict;
use warnings;
use Carp;
use vars qw($VERSION @ISA);

use DynaLoader ();
use Exporter ();

@ISA = qw(Exporter DynaLoader);

$VERSION = '0.0105';

=head1 NAME

MCDB_File - Perl extension for access to mcdb constant databases 

=head1 SYNOPSIS

    use MCDB_File ();
    tie %mcdb, 'MCDB_File', 'file.mcdb' or die "tie failed: $!\n";
    $value = $mcdb{$key};
    $num_records = scalar $mcdb;
    untie %mcdb;

    use MCDB_File ();
    eval {
      my $mcdb_make = new MCDB_File::Make('t.mcdb')
        or die "create t.mcdb failed: $!\n";
      $mcdb_make->insert('key1', 'value1');
      $mcdb_make->insert('key2' => 'value2', 'key3' => 'value3');
      $mcdb_make->insert(%t);
      $mcdb_make->finish;
    } or ($@ ne "" and warn "$@");

    use MCDB_File ();
    eval { MCDB_File::Make::create $file, %t; }
      or ($@ ne "" and warn "$@");

=head1 DESCRIPTION

B<MCDB_File> is a module which provides a Perl interface to B<mcdb>.
mcdb is originally based on Dan Bernstein's B<cdb> package.

    mcdb - fast, reliable, simple code to create, read constant databases

=head2 Reading from an mcdb constant database

After the C<tie> shown above, accesses to C<%h> will refer
to the B<mcdb> file C<file.mcdb>, as described in L<perlfunc/tie>.

C<keys>, C<values>, and C<each> can be used to iterate through records.
Note that only one iteration loop can be in progress at any one time.
Performing multiple iterations at the same time (i.e. in nested loops)
will not have independent iterators and therefore should be avoided.
Note that it is safe to use the find('key') method while iterating.
See PERFORMANCE section below for sample usage.

=head2 Creating an mcdb constant database

A B<mcdb> file is created in three steps.  First call
C<new MCDB_File::Make($fname)>, where C<$fname> is the name of the
database file to be created.  Secondly, call the C<insert> method
once for each (I<key>, I<value>) pair.  Finally, call the C<finish>
method to complete the creation.  A temporary file is used during
mcdb creation and atomically renamed to C<$fname> when C<finish>
method is successful.

Alternatively, call the C<insert()> method with multiple key/value
pairs. This can be significantly faster because there is less crossing
over the bridge from perl to C code. One simple way to do this is to pass
in an entire hash, as in: C<< $mcdb_make->insert(%hash); >>.

A simpler interface to B<cdb> file creation is provided by
C<MCDB_File::Make::create $fname, %t>.  This creates a B<mcdb> file named
C<$fname> containing the contents of C<%t>.

=head1 EXAMPLES

These are all complete programs.

1. Use $mcdb->find('key') method to look up a 'key' in an mcdb.

    use MCDB_File ();
    $mcdb = tie %h, MCDB_File, "$file.mcdb" or die ...;
    $value = $mcdb->find('key'); # slightly faster than $value = $h{key};
    undef $mcdb;
    untie %h;

2. Convert a Berkeley DB (B-tree) database to B<mcdb> format.

    use MCDB_File ();
    use DB_File;

    tie %h, DB_File, $ARGV[0], O_RDONLY, undef, $DB_BTREE
      or die "$0: can't tie to $ARGV[0]: $!\n";

    MCDB_File::Make::create $ARGV[1], %h;  # croak()s if error

3. Convert a flat file to B<mcdb> format.  In this example, the flat
file consists of one key per line, separated by a colon from the value.
Blank lines and lines beginning with B<#> are skipped.

    use MCDB_File;

    eval {
        my $mcdb = new MCDB_File::Make("data.mcdb")
          or die "$0: new MCDB_File::Make failed: $!\n";
        while (<>) {
            next if /^$/ or /^#/;
            chomp;
            ($k, $v) = split /:/, $_, 2;
            if (defined $v) {
                $mcdb->insert($k, $v);
            } else {
                warn "bogus line: $_\n";
            }
        }
        $mcdb->finish;
    } or ($@ ne "" and die "$@");

4. Perl version of B<mcdbctl dump>.

    use MCDB_File ();

    tie %data, 'MCDB_File', $ARGV[0]
      or die "$0: can't tie to $ARGV[0]: $!\n";
    while (($k, $v) = each %data) {
        print '+', length $k, ',', length $v, ":$k->$v\n";
    }
    print "\n";

5. Although a B<mcdb> file is constant, you can simulate updating it
in Perl.  This is an expensive operation, as you have to create a
new database, and copy into it everything that's unchanged from the
old database.  (As compensation, the update does not affect database
readers.  The old database is available for them, till the moment the
new one is C<finish>ed.)

    use MCDB_File ();

    $file = 'data.cdb';
    tie %old, 'MCDB_File', $file
      or die "$0: can't tie to $file: $!\n";
    $new = new MCDB_File::Make($file)
      or die "$0: new MCDB_File::Make failed: $!\n";

    eval {
        # Add the new values; remember which keys we've seen.
        while (<>) {
            chomp;
            ($k, $v) = split;
            $new->insert($k, $v);
            $seen{$k} = 1;
        }

        # Add any old values that haven't been replaced.
        while (($k, $v) = each %old) {
            $new->insert($k, $v) unless $seen{$k};
        }

        $new->finish;
    } or ($@ ne "" and die "$@");

=head1 REPEATED KEYS

Most users can ignore this section.

An B<mcdb> file can contain repeated keys.  If the C<insert> method is
called more than once with the same key during the creation of a B<mcdb>
file, that key will be repeated.

Here's an example.

    $mcdb = new MCDB_File::Make("$file.mcdb") or die ...;
    $mcdb->insert('cat', 'gato');
    $mcdb->insert('cat', 'chat');
    $mcdb->finish;

Normally, any attempt to access a key retrieves the first value
stored under that key.  This code snippet always prints B<gato>.

    $catref = tie %catalogue, MCDB_File, "$file.mcdb" or die ...;
    print "$catalogue{cat}";

However, all the usual ways of iterating over a hash---C<keys>,
C<values>, and C<each>---do the Right Thing, even in the presence of
repeated keys.  This code snippet prints B<cat cat gato chat>.

    print join(' ', keys %catalogue, values %catalogue);

And these two both print B<cat:gato cat:chat>, although the second is
more efficient.

    foreach $key (keys %catalogue) {
            print "$key:$catalogue{$key} ";
    } 

    while (($key, $val) = each %catalogue) {
            print "$key:$val ";
    }

The C<multi_get> method retrieves all the values associated with a key.
It returns a reference to an array containing all the values.  This code
prints B<gato chat>.

    print "@{$catref->multi_get('cat')}";

C<multi_get> always returns an array reference.  If the key was not
found in the database, it will be a reference to an empty array.  To
test whether the key was found, you must test the array, and not the
reference.

    $x = $catref->multi_get($key);
    warn "$key not found\n" unless $x; # WRONG; message never printed
    warn "$key not found\n" unless @$x; # Correct

Any extra references to C<MCDB_File> object (like C<$catref> in the
examples above) must be released with C<undef> or must have gone out of
scope before calling C<untie> on the hash.  This ensures that the object's
C<DESTROY> method is called.  Note that C<perl -w> will check this for
you; see L<perltie> for further details.

    use MCDB_File ();
    $catref = tie %catalogue, MCDB_File, "$file.mcdb" or die ...;
    print "@{$catref->multi_get('cat')}";
    undef $catref;
    untie %catalogue;

=head1 RETURN VALUES

The routines C<tie> and C<new> return B<undef> if the attempted
operation failed; C<$!> contains the reason for failure.
C<insert> and C<finish> call C<croak> if the attempted operation
fails.

=head1 DIAGNOSTICS

The following fatal errors may occur.
(See L<perlfunc/eval> if you want to trap them.)

=over 4

=item Modification of an MCDB_File attempted

You attempted to modify a hash tied to a B<MCDB_File>.

=item MCDB_File::Make::<insert|finish>:<error string>

An OS level problem occurred, such as permission denied writing
to filesystem, or you have run out of disk space.

=back

=head1 PERFORMANCE

Sometimes you need to get the most performance possible out of a
library. Rumour has it that perl's tie() interface is slow. In order
to get around that you can use MCDB_File in an object oriented
fashion, rather than via tie().

  my $mcdb = MCDB_File->TIEHASH('/path/to/mcdbfile.mcdb');
  if ($mcdb->EXISTS('key')) {
      print "Key: 'key'; Value: ", $mcdb->FETCH('key'), "\n";
  }
  undef $mcdb;

For more information on the methods available on tied hashes see L<perltie>.

Due to the internal Perl reuse of FETCH method to support queries,
as well as each() and values(), it will be sligthly more efficient
to call the $mcdb->find('key') method than to call $mcdb->FETCH('key').

=head1 ACKNOWLEDGEMENTS

mcdb is based on cdb, created by Dan Bernstein <djb@koobera.math.uic.edu>.
MCDB_File is based on CDB_File, created by Tim Goodwin, <tjg@star.le.ac.uk>
and currently maintained by Todd Rinaldo https://github.com/toddr/CDB_File/

=head1 AUTHOR

gstrauss  <code () gluelogic.com>

=cut

bootstrap MCDB_File $VERSION;

sub CLEAR {
	croak "Modification of an MCDB_File attempted"
}

sub DELETE {
	&CLEAR
}

sub STORE {
	&CLEAR
}

# Must be preloaded for the prototype.

package MCDB_File::Make;

sub create($\%) {
        my($fn, $RHdata) = @_;

        my $mcdb = new MCDB_File::Make($fn) or return undef;
        $mcdb->insert(%$RHdata);
        $mcdb->finish;
        return 1;
}

1;
