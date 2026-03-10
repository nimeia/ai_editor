if(WIN32)
  add_library(bridge_platform
    src/runtime_windows.cpp
  )
else()
  add_library(bridge_platform
    src/runtime_posix.cpp
  )
endif()

target_include_directories(bridge_platform PUBLIC include)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
