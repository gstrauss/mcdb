require 'mkmf'
require 'rbconfig'

case RbConfig::CONFIG['host_os']
when /linux/
  $LDFLAGS+= ' -Wl,--version-script,rubyext.map'
when /darwin/
  $LDFLAGS+= ' -load_hidden'
end

# compile and link against local, static libmcdb.a
$CPPFLAGS += ' -I ../../..'
$LDFLAGS  += ' ../../libmcdb.a'

have_library('mcdb')
$CPPFLAGS += ' -std=c99'
if RUBY_VERSION =~ /\A1\.8/ then
  $CPPFLAGS += " -DRUBY_1_8_x"
end

create_makefile('mcdb')

