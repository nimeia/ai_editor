cmake_minimum_required(VERSION 3.20)
project(ai_bridge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

enable_testing()

set(AI_BRIDGE_VERSION_HEADER_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in ${AI_BRIDGE_VERSION_HEADER_DIR}/bridge/core/version.hpp @ONLY)

add_subdirectory(libs/bridge_core)
add_subdirectory(libs/bridge_platform)
add_subdirectory(libs/bridge_transport)
add_subdirectory(apps/bridge_daemon)
add_subdirectory(apps/bridge_cli)
add_subdirectory(tests)

install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/ai_bridge)
include(cmake/Packaging.cmake)
