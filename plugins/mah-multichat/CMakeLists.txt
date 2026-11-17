cmake_minimum_required(VERSION 3.28...3.30)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_library(mah-multichat MODULE)
add_library(OBS::mah-multichat ALIAS mah-multichat)

target_sources(
  mah-multichat
  PRIVATE
    mah-multichat.cpp
    MahMultichatDock.cpp
    MahMultichatDock.hpp
    TwitchIrcClient.cpp
    TwitchIrcClient.hpp
    KickChatClient.cpp
    KickChatClient.hpp
    YouTubeLiveChatClient.cpp
    YouTubeLiveChatClient.hpp
    MiniWebSocket.cpp
    MiniWebSocket.hpp
)

target_link_libraries(
  mah-multichat
  PRIVATE OBS::libobs OBS::frontend-api Qt6::Widgets Qt6::Network $<$<PLATFORM_ID:Windows>:OBS::w32-pthreads>
)

set_target_properties(mah-multichat PROPERTIES AUTOMOC ON)

if(OS_WINDOWS)
  configure_file(cmake/windows/obs-module.rc.in mah-multichat.rc)
  target_sources(mah-multichat PRIVATE mah-multichat.rc)
endif()

set_target_properties_obs(mah-multichat PROPERTIES FOLDER plugins PREFIX "")
