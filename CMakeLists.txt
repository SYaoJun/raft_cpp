cmake_minimum_required(VERSION 3.5)
project(raft-kv)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-Wall)
add_definitions(-g)
add_definitions(-DDEBUG)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include_directories(${CMAKE_SOURCE_DIR})

find_package(msgpack REQUIRED)
find_package(glib REQUIRED)
find_package(hiredis REQUIRED)
# glib
if(GLIB_FOUND)
    include_directories(${GLIB_INCLUDE_DIRS})
    include_directories(${GLIBC_INCLUDE_DIRS})
    message(STATUS "glib FOUND!")
else()
    message(STATUS "glib NOT FOUND!")
endif()

# msgpack
if(MSGPACK_FOUND)
    message(STATUS "MSGPACK_FOUND!")
    include_directories(${MSGPACK_INCLUDE_DIRS})
else()
    message(STATUS "MSGPACK NOT FOUND!")
endif()
# hiredis
if(HIREDIS_FOUND)
    message(STATUS "HIREDIS_FOUND!")
    include_directories(${HIREDIS_INCLUDE_DIRS})
else()
    message(STATUS "HIREDIS NOT FOUND!")
endif()


find_package(Boost REQUIRED COMPONENTS system filesystem)

include_directories(${Boost_INCLUDE_DIRS})

set(LIBS
    ${HIREDIS_LIBRAIES}
    ${GLIB_LIBRARIES}
    Boost::system
    Boost::filesystem
    )

add_subdirectory(raft-kv)


# if (GoogleTest_FOUND)
#     include_directories(${GoogleTest_INCLUDE_DIRS})
#     set(LIBS
#         ${LIBS}
#         ${GoogleTest_LIBRARIES})
#     add_subdirectory(tests)
#     enable_testing()
# endif (GoogleTest_FOUND)

# configure_file(Procfile.in Procfile)
