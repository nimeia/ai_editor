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
install(FILES
  docs/00-doc-index.md
  docs/01-v1-scope-and-boundaries.md
  docs/02-v1-architecture-overview.md
  docs/03-v1-risks-and-notes.md
  docs/04-v1-protocol.md
  docs/05-v1-plan.md
  docs/06-v1-layout-and-interfaces.md
  docs/07-v1-build-run-test-guide.md
  docs/08-v1-release-and-deployment.md
  docs/09-v1-validation-report.md
  docs/10-v1-release-checklist.md
  docs/11-v1-text-editing-optimization-task.md
  docs/12-controlled-editing-task.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ai_bridge/docs)
install(PROGRAMS scripts/windows_smoke.ps1 scripts/package_release.sh scripts/package_release.ps1 scripts/validate_v1.sh scripts/validate_v1.ps1 DESTINATION ${CMAKE_INSTALL_DATADIR}/ai_bridge/scripts)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(AI_BRIDGE_PACKAGE_PLATFORM "macos")
elseif(WIN32)
  set(AI_BRIDGE_PACKAGE_PLATFORM "windows")
else()
  set(AI_BRIDGE_PACKAGE_PLATFORM "linux")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" AI_BRIDGE_PACKAGE_ARCH)
if(AI_BRIDGE_PACKAGE_ARCH STREQUAL "")
  set(AI_BRIDGE_PACKAGE_ARCH "unknown")
endif()

set(CPACK_PACKAGE_NAME "ai_bridge")
set(CPACK_PACKAGE_VENDOR "OpenAI")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform Agent Text Workspace Bridge")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "ai_bridge-${PROJECT_VERSION}-${AI_BRIDGE_PACKAGE_PLATFORM}-${AI_BRIDGE_PACKAGE_ARCH}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
if(WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
