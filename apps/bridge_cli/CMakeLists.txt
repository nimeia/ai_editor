add_executable(bridge_cli main.cpp)
target_link_libraries(bridge_cli PRIVATE bridge_core bridge_platform bridge_transport)
target_include_directories(bridge_cli PRIVATE ${AI_BRIDGE_VERSION_HEADER_DIR})
install(TARGETS bridge_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
