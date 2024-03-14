find_path(GLIB_INCLUDE_DIRS glib.h 
/opt/homebrew/Cellar/glib/2.80.0/include/glib-2.0 ${CMAKE_SOURCE_DIR}/ModuleMode)
find_path(GLIBC_INCLUDE_DIRS glibconfig.h
/opt/homebrew/Cellar/glib//2.80.0/lib/glib-2.0/include ${CMAKE_SOURCE_DIR}/ModuleMode)
find_library(GLIB_LIBRARIES NAMES glib-2.0 PATHS  /opt/homebrew/Cellar/glib/2.80.0/lib ${CMAKE_SOURCE_DIR}/ModuleMode)

message(STATUS "enter local cmake directory!glib-2.0")
if (GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES AND GLIBC_INCLUDE_DIRS)
    set(GLIB_FOUND TRUE)
endif ()