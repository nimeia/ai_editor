if(WIN32)
  add_library(bridge_transport
    src/transport_windows.cpp
  )
else()
  add_library(bridge_transport
    src/transport_posix.cpp
  )
endif()

target_include_directories(bridge_transport PUBLIC include)
if(UNIX)
  target_link_libraries(bridge_transport PUBLIC pthread)
endif()

