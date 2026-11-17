cmake_minimum_required(VERSION 3.28...3.30)

add_library(mah-mobile-cam MODULE)
add_library(OBS::mah-mobile-cam ALIAS mah-mobile-cam)

target_sources(mah-mobile-cam PRIVATE mah-mobile-cam.c)

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
