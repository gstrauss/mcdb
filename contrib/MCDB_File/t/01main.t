use strict;
use warnings;

use Test::More tests => 120;
use MCDB_File;

my $good_file_db = 'good.mcdb';

my %h;
ok(!(tie(%h, "MCDB_File", 'nonesuch.mcdb')), "Tie non-existant file");

my %a = qw(one Hello two Goodbye);
eval { MCDB_File::Make::create($good_file_db, %a) or die "Failed to create mcdb: $!" };
is("$@", '', "Create mcdb");

# Test that good file works.
tie(%h, "MCDB_File", $good_file_db) and pass("Test that good file works");

my $t = tied %h;
isa_ok($t, "MCDB_File" );
is($t->FETCH('one'), 'Hello', "Test that good file FETCHes right results");

is($h{'one'}, 'Hello', "Test that good file hash access gets right results");

ok(!defined($h{'1'}), "Check defined() non-existant entry works");

ok(exists($h{'two'}), "Check exists() on a real entry works");

ok(!exists($h{'three'}), "Check exists() on non-existant entry works");

my @h = sort keys %h;
is(scalar @h, 2, "keys length == 2");
is($h[0], 'one', "first key right");
is($h[1], 'two', "second key right");

eval { $h{'four'} = 'foo' };
like($@, qr/Modification of an MCDB_File attempted/, "Check modifying throws exception");

eval { delete $h{'five'} };
like($@, qr/Modification of an MCDB_File attempted/, "Check modifying throws exception");

undef $t;
untie %h; # Release the tie so the file closes and we can remove it.
cleanup_mcdb('good');

# Test empty file.
%a = ();
eval { MCDB_File::Make::create('empty.mcdb', %a) || die "Failed to create empty mcdb: $!" };
is("$@", '', "Create empty mcdb");

ok((tie(%h, "MCDB_File", 'empty.mcdb')), "Tie new empty mcdb");

@h = keys %h;
is(scalar @h, 0, "Empty mcdb has no keys");

untie %h;
cleanup_mcdb('empty');

# Test failing new.
ok(!MCDB_File::Make->new('..'), "Creating mcdb with dirs fails");

# Test file with repeated keys.
my $mcdbm = MCDB_File::Make->new('repeat.mcdb');
isa_ok($mcdbm, 'MCDB_File::Make');

$mcdbm->insert('dog', 'perro');
$mcdbm->insert('cat', 'gato');
$mcdbm->insert('cat', 'chat');
$mcdbm->insert('dog', 'chien');
$mcdbm->insert('rabbit', 'conejo');

$mcdbm->finish;
undef $mcdbm;

$t = tie %h, "MCDB_File", 'repeat.mcdb';
isa_ok($t, 'MCDB_File');

eval { $t->NEXTKEY('dog') };
# ok($@, qr/^Use MCDB_File::FIRSTKEY before MCDB_File::NEXTKEY/, "Test that NEXTKEY can't be used immediately after TIEHASH");
is($@, '', "Test that NEXTKEY can be used immediately after TIEHASH");

# Check keys/values works
my @k = keys %h;
my @v = values %h;
is($k[0], 'dog');     is($v[0], 'perro');
is($k[1], 'cat');     is($v[1], 'gato');
is($k[2], 'cat');     is($v[2], 'chat');
is($k[3], 'dog');     is($v[3], 'chien');
is($k[4], 'rabbit');  is($v[4], 'conejo');

@k = ();
@v = ();

# Check each works
while (my ($k, $v) = each %h) {
    push @k, $k;
    push @v, $v;
}
is($k[0], 'dog');     is($v[0], 'perro');
is($k[1], 'cat');     is($v[1], 'gato');
is($k[2], 'cat');     is($v[2], 'chat');
is($k[3], 'dog');     is($v[3], 'chien');
is($k[4], 'rabbit');  is($v[4], 'conejo');

my $v = $t->multi_get('cat');
is(@$v, 2, "multi_get returned 2 entries");
is($v->[0], 'gato');
is($v->[1], 'chat');

$v = $t->multi_get('dog');
is(@$v, 2, "multi_get returned 2 entries");
is($v->[0], 'perro');
is($v->[1], 'chien');

$v = $t->multi_get('rabbit');
is(@$v, 1, "multi_get returned 1 entry");
is($v->[0], 'conejo');

$v = $t->multi_get('foo');
is(ref($v), 'ARRAY', "multi_get on non-existant entry works");
is(@$v, 0);

while (my ($k, $v) = each %h) {
    $v = $t->multi_get($k);

    ok($v->[0] eq 'gato' and $v->[1] eq 'chat') if $k eq 'cat';
    ok($v->[0] eq 'perro' and $v->[1] eq 'chien') if $k eq 'dog';
    ok($v->[0] eq 'conejo') if $k eq 'rabbit';
}

# Test undefined keys.
{

    my $warned = 0;
    local $SIG{__WARN__} = sub { $warned = 1 if $_[0] =~ /^Use of uninitialized value/ };
    local $^W = 1;

    my $x;
    ok(! defined $h{$x});
    SKIP: {
	skip 'Perl 5.6 does not warn about $x{undef}', 1 unless $] > 5.007;
        ok($warned);
    }

    $warned = 0;
    ok(!exists $h{$x});
    SKIP: {
	skip 'Perl 5.6 does not warn about $x{undef}', 1 unless $] > 5.007;
	ok($warned);
    }

    $warned = 0;
    my $v = $t->multi_get('rabbit');
    ok($v);
    ok(! $warned);
}

# Check that object is readonly.
eval { $$t = 'foo' };
like($@, qr/^Modification of a read-only value/, "Check object (\$t) is read only");
is($h{'cat'}, 'gato');

undef $t;
untie %h;
cleanup_mcdb('repeat');

# Regression test - dumps core in 0.6.
%a = ('one', '');
ok((MCDB_File::Make::create($good_file_db, %a)), "Create good.mcdb");
ok((tie(%h, "MCDB_File", $good_file_db)), "Tie good.mcdb");
ok(!exists $h{'zero'}, "missing key test");

ok(defined($h{'one'}), "one is found and defined");
is($h{'one'}, '', "one is empty");

untie %h; # Release the tie so the file closes and we can remove it.
cleanup_mcdb('good');

# Test numeric data (broken before 0.8)
my $h = MCDB_File::Make->new('t.mcdb');
isa_ok($h, 'MCDB_File::Make');
$h->insert(1, 1 * 23);
eval { $h->finish; };
ok($@ eq "");
ok(tie(%h, "MCDB_File", 't.mcdb'));
is($h{1}, 23, "Numeric comparison works");

untie %h;
cleanup_mcdb('t');

# Test zero value with multi_get (broken before 0.85)
$h = MCDB_File::Make->new('t.mcdb');
isa_ok($h, 'MCDB_File::Make');
$h->insert('x', 0);
$h->insert('x', 1);
eval { $h->finish; };
ok($@ eq "");
$t = tie(%h, "MCDB_File", 't.mcdb');
isa_ok($t, 'MCDB_File');
my $x = $t->multi_get('x');
is(@$x, 2);
is($x->[0], 0);
is($x->[1], 1);

undef $t;
untie %h;
cleanup_mcdb('t');

$h = MCDB_File::Make->new('t.mcdb');
isa_ok($h, 'MCDB_File::Make');
for (my $i = 0; $i < 10; ++$i) {
    $h->insert($i, $i);
}
eval { $h->finish; };
ok($@ eq "");
undef $h;

$t = tie(%h, "MCDB_File", 't.mcdb');
isa_ok($t, 'MCDB_File');

for (my $i = 0; $i < 10; ++$i) {
    my ($k, $v) = each %h;
    if ($k == 2) {
        ok(exists($h{4}));
    }
    if ($k == 5) {
        ok(!exists($h{23}));
    }
    if ($k == 7) {
        my $m = $t->multi_get(3);
        is(@$m, 1);
        is($m->[0], 3);
    }
    is($k, $i, "$k eq $i");
    is($v, $i, "$v eq $i");
}
undef $t;
untie %h;
cleanup_mcdb('t');

sub cleanup_mcdb {
    my $file = shift;

    local $Test::Builder::Level = $Test::Builder::Level + 1;
    unlink "$file.mcdb", "$file.tmp";
    ok(!-e $_, "Remove $_") foreach("$file.mcdb", "$file.tmp");
}
