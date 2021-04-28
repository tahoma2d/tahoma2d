cmake_minimum_required(VERSION 3.19)
project(focal_distance_blur_plugin)
set(PLUGINSDK_ROOT ../../toonz/sources/toonzqt)
set(PLUGINSDK_UTILS_PATH ../)
set(PLUGIN_NAME FocalDistanceBlur)
set(PLUGIN_VENDOR TomDoingArt)

set(HEADERS
    ${PLUGINSDK_ROOT}/toonz_plugin.h
    ${PLUGINSDK_ROOT}/toonz_hostif.h
)

set(SOURCES
    focal_distance_blur.cpp
)

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
endif()

add_library(${PLUGIN_NAME} SHARED ${HEADERS} ${SOURCES})

set_target_properties(${PLUGIN_NAME} PROPERTIES
    PREFIX "${PLUGIN_VENDOR}_"
    SUFFIX ".plugin"
)

include_directories(${PLUGINSDK_ROOT} ${PLUGINSDK_UTILS_PATH})
