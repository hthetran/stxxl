############################################################################
#  tools/CMakeLists.txt
#
#  Part of the STXXL. See http://stxxl.org
#
#  Copyright (C) 2013 Timo Bingmann <tb@panthema.net>
#
#  Distributed under the Boost Software License, Version 1.0.
#  (See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt)
############################################################################

stxxl_build_tool(stxxl_tool
  benchmark_sort.cpp
  benchmark_pqueue.cpp
  mlock.cpp
  mallinfo.cpp
  )

install(TARGETS stxxl_tool RUNTIME DESTINATION ${INSTALL_BIN_DIR})

add_subdirectory(benchmarks)
add_subdirectory(extras)
