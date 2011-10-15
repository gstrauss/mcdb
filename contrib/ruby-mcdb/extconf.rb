require 'mkmf'

have_library('mcdb')
$CFLAGS += ' -std=c99'
if RUBY_VERSION =~ /\A1\.8/ then
  $CPPFLAGS += " -DRUBY_1_8_x"
end

create_makefile('mcdb')

