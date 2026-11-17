cmake_minimum_required(VERSION 3.28...3.30)

add_library(mah-mobile-cam MODULE)
add_library(OBS::mah-mobile-cam ALIAS mah-mobile-cam)

target_sources(
  mah-mobile-cam
  PRIVATE
    mah-mobile-cam.c
    mah-web.c
    mah-web.h
    mah-h264.c
    mah-h264.h
    stb_image.h
)

find_package(FFmpeg 8.0 COMPONENTS avcodec avutil)
if(FFmpeg_FOUND)
  target_compile_definitions(mah-mobile-cam PRIVATE MAH_HAVE_FFMPEG=1)
  target_link_libraries(mah-mobile-cam PRIVATE FFmpeg::avcodec FFmpeg::avutil)
  message(STATUS "mah-mobile-cam: H.264 decode enabled (FFmpeg)")
else()
  message(STATUS "mah-mobile-cam: H.264 decode disabled (FFmpeg not found; MJPEG still works)")
endif()

target_link_libraries(
  mah-mobile-cam
  PRIVATE
    OBS::libobs
    $<$<PLATFORM_ID:Windows>:OBS::w32-pthreads>
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:iphlpapi>
)

if(OS_WINDOWS)
  configure_file(cmake/windows/obs-module.rc.in mah-mobile-cam.rc)
  target_sources(mah-mobile-cam PRIVATE mah-mobile-cam.rc)
endif()

set_target_properties_obs(mah-mobile-cam PROPERTIES FOLDER plugins PREFIX "")
