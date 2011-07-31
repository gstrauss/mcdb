#!/bin/sh
#
# Requirements:
# You have softlimit in your path.
# XFSZ is signal 25.
#
# Some features not tested here:
# cdbmake traps 4GB overflows.


# djb cdb test and functions from djb cdb package (rts.tests)
# - some functions translated to use mcdbctl
# - some new tests added
function cdbdump {
  mcdbctl dump "$1"
}
function cdbget {
  mcdbctl get "$1" "$2" ${3+"$3"}
}
function cdbmake {
  mcdbctl make "$1" "$2"
}
function cdbstats {
  mcdbctl stats "$1"
}
function cdbtest {
  mcdbctl stats "$1" >/dev/null
}
function cdbmake_12 {
  awk '/^[^#]/ {print"+"length($1)","length($2)":"$1"->"$2} END {print""}' | \
    cdbmake "$1" "$2" 
}
function cdbmake_sv {
  awk '
    {
      if (split($0,x,"#")) {
        f = split(x[1],y)
        if (f >= 2) {
          if (split(y[2],z,"/") >= 2) {
            a = "@" z[1] "/" z[2]
            print "+" length(a) "," length(y[1]) ":" a "->" y[1]
            for (i = 1;i <= f;i += 1) {
              if (i != 2) {
                a = y[i] "/" z[2]
                print "+" length(a) "," length(z[1]) ":" a "->" z[1]
              }
            }
          }
        }
      }
    }
    END {
      print ""
    }
  ' | cdbmake "$1" "$2"
}


echo '--- cdbmake handles simple example'
echo '+3,5:one->Hello
+3,7:two->Goodbye
' | cdbmake test.cdb -
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbmake_12 handles simple example'
echo '
  one  Hello
# comment line
  two  Goodbye
' | cdbmake_12 12.cdb -
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbmake_sv handles simple example'
echo '
# Network services, Internet style
echo              7/tcp
echo              7/udp
discard           9/tcp    sink null
discard           9/udp    sink null
systat           11/tcp    users        #Active Users
systat           11/udp    users        #Active Users
' | cdbmake_sv sv.cdb -
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbdump handles simple examples'
cdbdump test.cdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbdump 12.cdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbdump sv.cdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbtest handles simple examples'
cdbtest test.cdb 
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbtest 12.cdb 
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbtest sv.cdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbstats handles simple examples'
cdbstats test.cdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbstats 12.cdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbstats sv.cdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbget retrieves data successfully'
cdbget test.cdb one >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbget test.cdb two >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
for i in @7 echo @9 discard sink null @11 systat users
do
  cdbget sv.cdb $i/tcp >/dev/null
  [ $? -eq 0 ] || echo 1>&2 "FAIL $?"
  cdbget sv.cdb $i/udp >/dev/null
  [ $? -eq 0 ] || echo 1>&2 "FAIL $?"
done

echo '--- cdbget exits 100 on nonexistent data'
cdbget test.cdb three >/dev/null
rc=$?; [ $rc -eq 100 ] || echo 1>&2 "FAIL $rc"
cdbget sv.cdb '#Active' >/dev/null
rc=$?; [ $rc -eq 100 ] || echo 1>&2 "FAIL $rc"


echo '--- cdbmake handles repeated keys'
echo '+3,5:one->Hello
+3,7:one->Goodbye
+3,7:one->Another
' | cdbmake rep.cdb -
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbdump rep.cdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbget handles repeated keys'
cdbget rep.cdb one >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbget rep.cdb one 0 >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbget rep.cdb one 1 >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbget rep.cdb one 2 >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbget rep.cdb one 3 >/dev/null
rc=$?; [ $rc -eq 100 ] || echo 1>&2 "FAIL $rc"
cdbget rep.cdb one 4 >/dev/null
rc=$?; [ $rc -eq 100 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbmake handles long keys and data'
echo '+320,320:ba483b3442e75cace82def4b5df25bfca887b41687537c21dc4b82cb4c36315e2f6a0661d1af2e05e686c4c595c16561d8c1b3fbee8a6b99c54b3d10d61948445298e97e971f85a600c88164d6b0b09
b5169a54910232db0a56938de61256721667bddc1c0a2b14f5d063ab586a87a957e87f704acb7246c5e8c25becef713a365efef79bb1f406fecee88f3261f68e239c5903e3145961eb0fbc538ff506a
->152e113d5deec3638ead782b93e1b9666d265feb5aebc840e79aa69e2cfc1a2ce4b3254b79fa73c338d22a75e67cfed4cd17b92c405e204a48f21c31cdcf7da46312dc80debfbdaf6dc39d74694a711
6d170c5fde1a81806847cf71732c7f3217a38c6234235951af7b7c1d32e62d480d7c82a63a9d94291d92767ed97dd6a6809d1eb856ce23eda20268cb53fda31c016a19fc20e80aec3bd594a3eb82a5a

' | cdbmake test.cdb -
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbdump handles long keys and data'
cdbdump test.cdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbtest handles long keys and data'
cdbtest test.cdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbstats handles long keys and data'
cdbstats test.cdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbget handles long keys and data'
cdbget test.cdb 'ba483b3442e75cace82def4b5df25bfca887b41687537c21dc4b82cb4c36315e2f6a0661d1af2e05e686c4c595c16561d8c1b3fbee8a6b99c54b3d10d61948445298e97e971f85a600c88164d6b0b09
b5169a54910232db0a56938de61256721667bddc1c0a2b14f5d063ab586a87a957e87f704acb7246c5e8c25becef713a365efef79bb1f406fecee88f3261f68e239c5903e3145961eb0fbc538ff506a
' >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbmake handles unreadable input'
ln -s loop loop
echo '' | cdbmake test.cdb loop 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbmake handles unmovable cdb'
echo '' | cdbmake loop/test.cdb - 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc"


if which softlimit 2>/dev/null; then

echo '--- cdbmake handles nomem'
perl -e 'for (1..5000) { print "+3,5:one->Hello\n"; }' \
| softlimit -d 50000 cdbmake test.cdb - 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbmake handles full disk'
(
  trap '' 25
  echo '' | softlimit -f 2047 cdbmake test.cdb - 2>/dev/null
  [ $? -eq 111 ] || echo 1>&2 "FAIL $?"
)

fi


echo '--- cdbmake handles absurd klen'
echo '+4294967210' | cdbmake test.cdb - 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbget handles empty file'
touch empty.cdb
cdbget empty.cdb foo 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc"


echo '--- cdbmake and cdbdump handle random.cdb'
cdbmake random.cdb - < ../random.in
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbdump random.cdb > random.dump
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cmp ../random.in random.dump >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbtest handles random.cdb'
cdbtest random.cdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- cdbstats handles random.cdb'
cdbstats random.cdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"


echo '--- testzero works'
testzero 5 test.cdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cdbtest test.cdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- testzero can build a database very close to 4GB'
testzero 65507
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- testzero handles hash table crossing 4GB'
testzero 65508
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- testzero handles records past 4GB'
testzero 66000
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"


exit 0
