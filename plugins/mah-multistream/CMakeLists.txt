cmake_minimum_required(VERSION 3.28...3.30)

find_package(Qt6 REQUIRED Widgets)

add_library(mah-multistream MODULE)
add_library(OBS::mah-multistream ALIAS mah-multistream)

target_sources(
  mah-multistream
  PRIVATE
    src/edit-widget.cpp
    src/edit-widget.h
    src/helpers.cpp
    src/helpers.h
    src/json-util.hpp
    src/obs-multi-rtmp.cpp
    src/obs-multi-rtmp.h
    src/obs-properties-widget.cpp
    src/obs-properties-widget.h
    src/output-config.cpp
    src/output-config.h
    src/pch.h
    src/plugin-macros.h
    src/plugin-support.c
    src/plugin-support.h
    src/protocols.cpp
    src/protocols.h
    src/push-widget.cpp
    src/push-widget.h
)

target_include_directories(
  mah-multistream
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/dep/nlohmann-json"
)

target_link_libraries(
  mah-multistream
  PRIVATE OBS::libobs OBS::frontend-api Qt6::Widgets $<$<PLATFORM_ID:Windows>:OBS::w32-pthreads>
)

set_target_properties(
  mah-multistream
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)

if(OS_WINDOWS)
  configure_file(cmake/windows/obs-module.rc.in mah-multistream.rc)
  target_sources(mah-multistream PRIVATE mah-multistream.rc)
endif()

set_target_properties_obs(mah-multistream PROPERTIES FOLDER plugins PREFIX "")
