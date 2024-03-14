find_path(HIREDIS_INCLUDE_DIRS hiredis.h 
/opt/homebrew/Cellar/hiredis/1.2.0/include/hiredis/ ${CMAKE_SOURCE_DIR}/ModuleMode)

find_library(HIREDIS_LIBRAIES NAMES hiredis PATHS  /opt/homebrew/Cellar/hiredis/1.2.0/lib ${CMAKE_SOURCE_DIR}/ModuleMode)

message(STATUS "enter local cmake directory!hiredis")
if (HIREDIS_INCLUDE_DIRS AND HIREDIS_LIBRAIES)
    set(HIREDIS_FOUND TRUE)
endif ()