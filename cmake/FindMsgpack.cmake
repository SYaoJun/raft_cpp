find_path(MSGPACK_INCLUDE_DIRS msgpack.hpp /opt/homebrew/Cellar/msgpack-cxx/6.1.0/include ${CMAKE_SOURCE_DIR}/ModuleMode)

message(STATUS "enter local cmake directory! msgpack-cxx")
if (MSGPACK_INCLUDE_DIRS)
    set(MSGPACK_FOUND TRUE)
endif (MSGPACK_INCLUDE_DIRS)