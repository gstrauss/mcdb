require 'mcdb.so'

class MCDBmake

  # 'create' convenience method which is called on a block and then finishes
  # mcdb creation unless an exception is thrown or caller calls 'break' from
  # within the block.
  def MCDBmake.create(file, mode = 0400, do_fsync = true)
    MCDBmake.open(file, mode) do |mk|
      yield mk
      mk.finish(do_fsync)
    end
  end

end
