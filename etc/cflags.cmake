set (CMAKE_CXX_STANDARD 17)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -pedantic -pedantic-errors -Werror -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-qual -Wformat=2 -Weffc++ -Wold-style-cast")

# check for supported compiler versions
set (IS_GNU_COMPILER ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
set (IS_CLANG_COMPILER ("${CMAKE_CXX_COMPILER_ID}" MATCHES "[Cc][Ll][Aa][Nn][Gg]"))
set (CXX_VERSION_LT_6 ("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 6))
set (CXX_VERSION_LT_8 ("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 8))
if ((${IS_GNU_COMPILER} AND ${CXX_VERSION_LT_8}) OR (${IS_CLANG_COMPILER} AND ${CXX_VERSION_LT_6}))
    message (FATAL_ERROR "You must compile this project with g++ >= 8 or clang >= 6.")
endif ()
if (${IS_CLANG_COMPILER})
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wloop-analysis")
endif ()

# add some flags for the Release, Debug, and DebugSan modes
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3 -Og")
set (CMAKE_CXX_FLAGS_DEBUGASAN "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined -fsanitize=address")
set (CMAKE_CXX_FLAGS_RELASAN "${CMAKE_CXX_FLAGS_RELEASE} -fsanitize=undefined -fsanitize=address")
