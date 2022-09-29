#!/usr/bin/ruby

require 'mcdb'

require 'lib/mcdb.rb'  # required only during testing from ruby-mcdb build area

require 'pp'           # pretty print

MCDBmake.create('t.mcdb') do |m|
  m['a'] = 'b'
  m.add('c','d')
  m.add('c','dd')
  m['e'] = 'f'
  m.add('g','h')
end

m = MCDBmake.open('t.mcdb')
m['x'] = 'y'
m.cancel

m = MCDB.open('t.mcdb')
print %(#{m['x']}\n)
print %(#{m['g']}\n)
m.close

# This block is less an example and more some basic tests exercising methods
MCDB.open('t.mcdb') do |m|
  m.madvise(MCDB::MADV_RANDOM)
  pp m.empty?
  pp m.findall('c')
  print %(#{m['c']} #{m.findkey('c')} #{m.findnext} [#{m.findnext}]\n)
  print %(#{m.getseq('c')}\n)
  print %(#{m.getseq('c', 0)}\n)
  print %(#{m.getseq('c', 1)}\n)
  print %(#{m.getseq('c', 2)}\n)
  print %(#{m.get('j', 'x')}\n)
  print %(#{m.fetch('a')}\n)
  print %(#{m.fetch('a', 'x')}\n)
  pp m.key?('j')
  pp m.has_key?('a')
  pp m.value?('j')
  pp m.has_value?('d')
  print %(#{m.size}\n)
  print %(#{m.count}\n)
  print %(#{m.count('c')}\n)
  m.count { |x,| /t/.match(x) }
  print %(#{m.count { |x,| /t/.match(x) }}\n)
  print %(#{m.count { |x,| /c/.match(x) }}\n)
  m.each       do |k,d|  print %(k:#{k} d:#{d}\n); end
  m.each_pair  do |k,d|  print %(k:#{k} d:#{d}\n); end
  m.each_key   do |data| print %(k:#{data}\n); end
  m.each_value do |data| print %(v:#{data}\n); end
  m.each('c')  do |data| print %(#{data}\n); end
  pp m.values_at('c', 'e')
  y = m.select do |x,| /c/.match(x); end
  pp y
  z = m.reject do |x,| /c/.match(x); end
  pp z
  pp m.keys
  pp m.values
  pp m.pairs
  pp m.to_a
  pp m.to_hash
  pp m.invert
  print %(#{m.index('h')}\n)
end

print %(\n\n)

# modified from ruby-cdb-0.5a by Kazuteru Okahashi <okahashi@gmail.com>
# (combines from ruby-cdb-0.5a/sample/sample.rb and ruby-cdb-0.5a/lib/cdb.rb)

# example class MCDBex wraps some methods from class MCDB and class MCDBmake
class MCDBex

  def MCDBex.dump(file)
    MCDB.open(file) do |m|
      m.madvise(MCDB::MADV_SEQUENTIAL)
      m.each do |k, d|
        print %(+#{k.length},#{d.length}:#{k}->#{d}\n)
      end
      print "\n"
    end
  end

  def MCDBex.each(file, key = nil)
    MCDB.open(file) do |db|
      if key
        db.each(key) do |d|
          yield d
        end
      else
        db.each do |k, d|
          yield k, d
        end
      end
    end
  end

  def MCDBex.edit(file, mode = nil)
    MCDB.open(file) do |m|
      ary = m.to_a
      yield ary
      MCDBmake.open(file, mode) do |mk|
        ary.each do |k, d|
          mk.add(k, d)
        end
        mk.finish
      end
    end
  end

  def MCDBex.update(file, mode = nil)
    MCDB.open(file) do |m|
      MCDBmake.open(file, mode) do |mk|
        yield(m, mk)
        mk.finish
      end
    end
  end

end


MCDBmake.create('t.mcdb') do |data|
  data['eins'] = 'ichi'
  data['zwei'] = 'ni'
  data['drei'] = 'san'
  data['vier'] = 'yon'
end

MCDBex.dump('t.mcdb')

MCDBex.update('t.mcdb') do |read, write|
  write['one']   = read['eins']
  write['two']   = read['zwei']
  write['three'] = read['drei']
  write['four']  = read['vier']
end

MCDBex.dump('t.mcdb')

MCDBex.edit('t.mcdb') do |data|
  data.push ['five',  'go' ]
  data.push ['six',   'roku' ]
  data.push ['seven', 'nana' ]
  data.push ['eight', 'hachi' ]
  data.delete_if { |a,| /t/.match(a) }
end

MCDBex.dump('t.mcdb')
