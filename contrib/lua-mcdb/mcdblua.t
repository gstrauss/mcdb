#!/bin/sh

function lua_mcdb_make {
  typeset fn="$1"
  [ -n "$fn" ] || exit 111;

  sed '/^#/d' | lua -e "
    local mcdb_make = require('mcdb_make')
    local function fatal(msg)
        io.stderr:write('lua_mcdb_make: ' .. (msg or '') .. '\\n')
        os.exit(111)
    end
    local mk, errstr = mcdb_make.init('"$fn"')
    if mk == nil then fatal('mcdb_make.init: ' .. errstr) end

    while 1 do
        local line = io.read('*l')
        if line == nil or #line == 0 then break end
        local _, last, klen, dlen = line:find('^[+](%d+),(%d+):')
        if last == nil or #line ~= (last + klen + 2 + dlen)
           or line:sub(last+klen+1, last+klen+2) ~= '->' then
            fatal('mk input line error')
        end
        local key  = line:sub(last+1, last+klen)
        local data = line:sub(last+klen+3, last+klen+2+dlen)
        --   errstr = mk:add(key, data)
        --   if errstr then fatal('mk:add: ' .. errstr) end
        mk[key] = data
    end

    errstr = mk:finish()
    if errstr then fatal('mk:finish: ' .. errstr) end
"
}


function lua_mcdb_dump {
  typeset fn="$1"
  [ -n "$fn" ] || exit 111;

  lua - <<EOF
    local mcdb = require("mcdb")
    local m, errstr = mcdb.init("$fn")
    if m == nil then
        io.stderr:write('lua_mcdb_dump: mcdb.init: ' .. errstr .. '\n')
        os.exit(111)
    end

    for k,v in m:iteritems() do
        io.write(string.format('+%d,%d:',#k,#v), k, '->', v, '\n')
    end
    io.write('\n')
EOF
}


function lua_mcdb_get {
  typeset fn key skip
  case $# in
   2) fn="$1"; key="$2"; skip=0;;
   3) fn="$1"; key="$2"; skip="$3";;
   *) echo 1>&2 "lua_mcdb_get: usage: lua_mcdb_get key [skip]"; exit 111;;
  esac

  lua - <<EOF
    local mcdb = require("mcdb")
    local m, errstr = mcdb.init("$fn")
    if m == nil then
        io.stderr:write('lua_mcdb_get: mcdb.init: ' .. errstr .. '\n')
        os.exit(111)
    end

    -- get specific sequence instance of multi-value key in mcdb
    --   local val = m:getseq("$key", "$skip"+0)
    -- simpler query for first (or single) key:
    --   local val = m:find("$key")
    -- even simpler query, employing __call internal metamethod
    local val = m("$key")

    -- local val = m["$key"] does not work as desired due to limitations in lua
    -- (It will return nil *unless* key matches mcdb instance method name (!),
    --  neither of which is the dictionary lookup usually intended.  Beware.)

    if val then io.write(val); os.exit(0); else os.exit(100) end
EOF
}



err=0

# create an mcdb
# (input chunk produced from mcdbmake_sv function in mcdb/t/mcdbctl.t)
echo '+6,4:@7/tcp->echo
+8,1:echo/tcp->7
+6,4:@7/udp->echo
+8,1:echo/udp->7
+6,7:@9/tcp->discard
+11,1:discard/tcp->9
+8,1:sink/tcp->9
+8,1:null/tcp->9
+6,7:@9/udp->discard
+11,1:discard/udp->9
+8,1:sink/udp->9
+8,1:null/udp->9
+7,6:@11/tcp->systat
+10,2:systat/tcp->11
+9,2:users/tcp->11
+7,6:@11/udp->systat
+10,2:systat/udp->11
+9,2:users/udp->11

' | lua_mcdb_make services.mcdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc: lua_mcdb_make services.mcdb"
err=$(($err+$rc))

rv="`lua_mcdb_get services.mcdb echo/tcp`"
rc=$?; [ $rc -eq 0 ] && [ "$rv" = '7' ] || rc=101
[ $rc -eq 0 ] || echo 1>&2 "FAIL $rc: lua_mcdb_get services.mcdb echo/tcp"
err=$(($err+$rc))

rv="`lua_mcdb_get services.mcdb @7/tcp`"
rc=$?; [ $rc -eq 0 ] && [ "$rv" = 'echo' ] || rc=101
[ $rc -eq 0 ] || echo 1>&2 "FAIL $rc: lua_mcdb_get services.mcdb @7/tcp"
err=$(($err+$rc))

lua_mcdb_get services.mcdb xxx >/dev/null
rc=$?; [ $rc -eq 100 ] || echo 1>&2 "FAIL $rc: lua_mcdb_get services.mcdb xxx"
[ $rc -eq 100 ] && rc=0
err=$(($err+$rc))

lua_mcdb_get /dev/null xxx 2>/dev/null
rc=$?; [ $rc -eq 111 ] || echo 1>&2 "FAIL $rc: lua_mcdb_get /dev/null xxx"
[ $rc -eq 111 ] && rc=0
err=$(($err+$rc))

echo '+3,5:one->Hello
# comment line
+3,7:two->Goodbye' | lua_mcdb_make 12.mcdb
rc=$?; [ $rc -eq 0 ] || echo 1>&2 "FAIL $rc: lua_mcdb_make 12.mcdb"
err=$(($err+$rc))

rv="`lua_mcdb_dump 12.mcdb | wc -l`"
rc=$?; [ $rc -eq 0 ] && [ "$rv" = '3' ] || rc=101
[ $rc -eq 0 ] || echo 1>&2 "FAIL $rc: lua_mcdb_dump 12.mcdb"
err=$(($err+$rc))


/bin/rm -f 12.mcdb services.mcdb
exit $err
