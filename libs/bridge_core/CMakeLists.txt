add_library(bridge_core
  src/instance.cpp
  src/workspace.cpp
  src/path_policy.cpp
  src/file_service.cpp
  src/search_service.cpp
  src/patch_service.cpp
  src/logging.cpp
  src/error_codes.cpp
  src/protocol.cpp
)

target_include_directories(bridge_core PUBLIC include ${AI_BRIDGE_VERSION_HEADER_DIR})
target_link_libraries(bridge_core PUBLIC stdc++fs)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
