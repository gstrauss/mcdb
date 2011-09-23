#!/bin/sh
#
# Requirements:
# You have softlimit in your path.
# XFSZ is signal 25.
#
# Some features not tested here:
# mcdbmake traps 4GB overflows.


# djb cdb test and functions from djb cdb package (rts.tests)
# - some functions translated to use mcdbctl
# - some new tests added
function mcdbdump {
  mcdbctl dump "$1"
}
function mcdbget {
  mcdbctl get "$1" "$2" ${3+"$3"}
}
function mcdbmake {
  mcdbctl make "$1" "$2"
}
function mcdbstats {
  mcdbctl stats "$1"
}
function mcdbtest {
  mcdbctl stats "$1" >/dev/null
}
function mcdbmake_12 {
  awk '/^[^#]/ {print"+"length($1)","length($2)":"$1"->"$2} END {print""}' | \
    mcdbmake "$1" "$2" 
}
function mcdbmake_sv {
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
  ' | mcdbmake "$1" "$2"
}


echo '--- mcdbmake handles simple example'
echo '+3,5:one->Hello
+3,7:two->Goodbye
' | mcdbmake test.mcdb -
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbmake_12 handles simple example'
echo '
  one  Hello
# comment line
  two  Goodbye
' | mcdbmake_12 12.mcdb -
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbmake_sv handles simple example'
echo '
# Network services, Internet style
echo              7/tcp
echo              7/udp
discard           9/tcp    sink null
discard           9/udp    sink null
systat           11/tcp    users        #Active Users
systat           11/udp    users        #Active Users
' | mcdbmake_sv sv.mcdb -
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbdump handles simple examples'
mcdbdump test.mcdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbdump 12.mcdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbdump sv.mcdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbtest handles simple examples'
mcdbtest test.mcdb 
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbtest 12.mcdb 
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbtest sv.mcdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbstats handles simple examples'
mcdbstats test.mcdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbstats 12.mcdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbstats sv.mcdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbget retrieves data successfully'
mcdbget test.mcdb one >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbget test.mcdb two >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
for i in @7 echo @9 discard sink null @11 systat users
do
  mcdbget sv.mcdb $i/tcp >/dev/null
  [ $? -eq 0 ] || echo 1>&2 "FAIL $?"
  mcdbget sv.mcdb $i/udp >/dev/null
  [ $? -eq 0 ] || echo 1>&2 "FAIL $?"
done

echo '--- mcdbget exits 100 on nonexistent data'
mcdbget test.mcdb three >/dev/null
rc=$?; [ $rc -eq 100 ] || echo 1>&2 "FAIL $rc"
mcdbget sv.mcdb '#Active' >/dev/null
rc=$?; [ $rc -eq 100 ] || echo 1>&2 "FAIL $rc"


echo '--- mcdbmake handles repeated keys'
echo '+3,5:one->Hello
+3,7:one->Goodbye
+3,7:one->Another
' | mcdbmake rep.mcdb -
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbdump rep.mcdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbget handles repeated keys'
mcdbget rep.mcdb one >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbget rep.mcdb one 0 >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbget rep.mcdb one 1 >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbget rep.mcdb one 2 >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbget rep.mcdb one 3 >/dev/null
rc=$?; [ $rc -eq 100 ] || echo 1>&2 "FAIL $rc"
mcdbget rep.mcdb one 4 >/dev/null
rc=$?; [ $rc -eq 100 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbmake handles long keys and data'
echo '+320,320:ba483b3442e75cace82def4b5df25bfca887b41687537c21dc4b82cb4c36315e2f6a0661d1af2e05e686c4c595c16561d8c1b3fbee8a6b99c54b3d10d61948445298e97e971f85a600c88164d6b0b09
b5169a54910232db0a56938de61256721667bddc1c0a2b14f5d063ab586a87a957e87f704acb7246c5e8c25becef713a365efef79bb1f406fecee88f3261f68e239c5903e3145961eb0fbc538ff506a
->152e113d5deec3638ead782b93e1b9666d265feb5aebc840e79aa69e2cfc1a2ce4b3254b79fa73c338d22a75e67cfed4cd17b92c405e204a48f21c31cdcf7da46312dc80debfbdaf6dc39d74694a711
6d170c5fde1a81806847cf71732c7f3217a38c6234235951af7b7c1d32e62d480d7c82a63a9d94291d92767ed97dd6a6809d1eb856ce23eda20268cb53fda31c016a19fc20e80aec3bd594a3eb82a5a

' | mcdbmake test.mcdb -
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbdump handles long keys and data'
mcdbdump test.mcdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbtest handles long keys and data'
mcdbtest test.mcdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbstats handles long keys and data'
mcdbstats test.mcdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbget handles long keys and data'
mcdbget test.mcdb 'ba483b3442e75cace82def4b5df25bfca887b41687537c21dc4b82cb4c36315e2f6a0661d1af2e05e686c4c595c16561d8c1b3fbee8a6b99c54b3d10d61948445298e97e971f85a600c88164d6b0b09
b5169a54910232db0a56938de61256721667bddc1c0a2b14f5d063ab586a87a957e87f704acb7246c5e8c25becef713a365efef79bb1f406fecee88f3261f68e239c5903e3145961eb0fbc538ff506a
' >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbmake handles unreadable input'
ln -s loop loop
echo '' | mcdbmake test.mcdb loop 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbmake handles unmovable cdb'
echo '' | mcdbmake loop/test.mcdb - 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc"


if which softlimit >/dev/null 2>&1; then

echo '--- mcdbmake handles nomem'
perl -e 'for (1..5000) { print "+3,5:one->Hello\n"; }' \
| softlimit -d 50000 mcdbmake test.mcdb - 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbmake handles full disk'
(
  trap '' 25
  echo '' | softlimit -f 2047 mcdbmake test.mcdb - 2>/dev/null
  [ $? -eq 111 ] || echo 1>&2 "FAIL $?"
)

fi


echo '--- mcdbmake handles absurd klen'
echo '+4294967210' | mcdbmake test.mcdb - 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbget handles empty file'
touch empty.mcdb
mcdbget empty.mcdb foo 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc"


echo '--- mcdbmake and mcdbdump handle random.mcdb'
mcdbmake random.mcdb - < ../random.in
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbdump random.mcdb > random.dump
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
cmp ../random.in random.dump >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbtest handles random.mcdb'
mcdbtest random.mcdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

echo '--- mcdbstats handles random.mcdb'
mcdbstats random.mcdb >/dev/null
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"


echo '--- testzero works'
testzero 5 test.mcdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
mcdbtest test.mcdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

# testzero uses keys of size 4 and data records of size 65536
# mcdb overhead is 4096 + 24 bytes per record as long as data section fits <4 GB
echo '--- testzero can build a database very close to 4GB'
testzero 65507
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

# testzero on large data sets will fail in 32-bit, but should succeed in 64-bit
if [ -n "$1" ]; then
  echo '--- testzero handles hash table crossing 4GB'
  testzero 65508
  rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"

  echo '--- testzero handles records past 4GB'
  testzero 65529
  rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc"
fi


exit 0
