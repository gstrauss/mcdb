use strict;
use warnings;

use Test::More tests => 44;
use MCDB_File;

my $c = MCDB_File::Make->new('last.mcdb');
isa_ok($c, 'MCDB_File::Make');

for (1..10) {
    $c->insert("Key$_" => "Val$_");
}

eval { $c->finish; };
is($@, "", "Finish writes out");

my %h;
tie(%h, "MCDB_File", "last.mcdb");
isa_ok(tied(%h), 'MCDB_File');
my $count = 0;

foreach my $k (keys %h) {
    $k =~ m/^Key(\d+)$/ or die;
    my $n = $1;
    ok($n <= 10 && $n > 0, "Expected key ($n) is found")
        or diag($k);
    
    is($h{$k}, "Val$n", "Val$n matches");
}

tie(%h, "MCDB_File", "last.mcdb");
isa_ok(tied(%h), 'MCDB_File');

while (my ($k, $v) = each(%h)) {
    ok($k, "verify k in re-tied hash ($k)");
    ok($v, "verify v in re-tied hash ($v)");
}

END { unlink 'last.mcdb' }
