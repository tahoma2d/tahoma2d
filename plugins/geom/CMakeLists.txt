project(geom_plugin)

set(HEADERS
    ../../toonz/sources/toonzqt/toonz_plugin.h
    ../../toonz/sources/toonzqt/toonz_hostif.h)

set(SOURCES
    geom.cpp)

if (APPLE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
endif (APPLE)

add_library(geom SHARED ${HEADERS} ${SOURCES})

set_target_properties(geom PROPERTIES
    PREFIX ""
    SUFFIX ".plugin")

include_directories(../../toonz/sources/toonzqt ../)
