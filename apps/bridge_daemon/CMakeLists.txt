add_executable(bridge_daemon main.cpp)
target_link_libraries(bridge_daemon PRIVATE bridge_core bridge_platform bridge_transport)
target_include_directories(bridge_daemon PRIVATE ${AI_BRIDGE_VERSION_HEADER_DIR})
install(TARGETS bridge_daemon RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
