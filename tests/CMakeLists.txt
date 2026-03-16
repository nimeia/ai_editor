add_executable(test_instance tests_instance.cpp)
target_link_libraries(test_instance PRIVATE bridge_core bridge_platform)
add_test(NAME test_instance COMMAND test_instance)

add_executable(test_path_policy tests_path_policy.cpp)
target_link_libraries(test_path_policy PRIVATE bridge_core)
add_test(NAME test_path_policy COMMAND test_path_policy)

add_executable(test_file_service tests_file_service.cpp)
target_link_libraries(test_file_service PRIVATE bridge_core)
add_test(NAME test_file_service COMMAND test_file_service)

add_executable(test_file_stream tests_file_stream.cpp)
target_link_libraries(test_file_stream PRIVATE bridge_core)
add_test(NAME test_file_stream COMMAND test_file_stream)

add_executable(test_search_service tests_search_service.cpp)
target_link_libraries(test_search_service PRIVATE bridge_core)
add_test(NAME test_search_service COMMAND test_search_service)

add_executable(test_patch_service tests_patch_service.cpp)
target_link_libraries(test_patch_service PRIVATE bridge_core)
add_test(NAME test_patch_service COMMAND test_patch_service)

add_executable(test_platform_transport tests_platform_transport.cpp)
target_link_libraries(test_platform_transport PRIVATE bridge_platform bridge_transport)
add_test(NAME test_platform_transport COMMAND test_platform_transport)

add_executable(test_logging tests_logging.cpp)
target_link_libraries(test_logging PRIVATE bridge_core)
add_test(NAME test_logging COMMAND test_logging)

add_executable(test_error_codes tests_error_codes.cpp)
target_link_libraries(test_error_codes PRIVATE bridge_core)
add_test(NAME test_error_codes COMMAND test_error_codes)

add_executable(test_file_service_edges tests_file_service_edges.cpp)
target_link_libraries(test_file_service_edges PRIVATE bridge_core)
add_test(NAME test_file_service_edges COMMAND test_file_service_edges)

add_executable(test_search_service_edges tests_search_service_edges.cpp)
target_link_libraries(test_search_service_edges PRIVATE bridge_core)
add_test(NAME test_search_service_edges COMMAND test_search_service_edges)

add_executable(test_patch_service_edges tests_patch_service_edges.cpp)
target_link_libraries(test_patch_service_edges PRIVATE bridge_core)
add_test(NAME test_patch_service_edges COMMAND test_patch_service_edges)

add_test(NAME test_bridge_cli_version COMMAND $<TARGET_FILE:bridge_cli> --version)
add_test(NAME test_bridge_daemon_version COMMAND $<TARGET_FILE:bridge_daemon> --version)

if(WIN32)
  find_program(AI_BRIDGE_POWERSHELL NAMES pwsh powershell REQUIRED)
  add_test(
    NAME test_functional_windows_native
    COMMAND ${AI_BRIDGE_POWERSHELL} -File ${CMAKE_CURRENT_SOURCE_DIR}/functional_windows_native.ps1 $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_windows_native PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_windows_stream_cancel_timeout
    COMMAND ${AI_BRIDGE_POWERSHELL} -File ${CMAKE_CURRENT_SOURCE_DIR}/functional_windows_stream_cancel_timeout.ps1 $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_windows_stream_cancel_timeout PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_windows_release_package
    COMMAND ${AI_BRIDGE_POWERSHELL} -File ${CMAKE_CURRENT_SOURCE_DIR}/functional_windows_release_package.ps1 $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_windows_release_package PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_windows_cli_contract
    COMMAND ${AI_BRIDGE_POWERSHELL} -File ${CMAKE_CURRENT_SOURCE_DIR}/functional_windows_cli_contract.ps1 $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_windows_cli_contract PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_windows_logging_codes
    COMMAND ${AI_BRIDGE_POWERSHELL} -File ${CMAKE_CURRENT_SOURCE_DIR}/functional_windows_logging_codes.ps1 $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_windows_logging_codes PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_windows_logging_success
    COMMAND ${AI_BRIDGE_POWERSHELL} -File ${CMAKE_CURRENT_SOURCE_DIR}/functional_windows_logging_success.ps1 $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_windows_logging_success PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")
else()
  add_test(
    NAME test_integration_ping_info
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_ping_info.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_ping_info PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_file_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_file_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_file_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_search_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_search_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_search_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_patch_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_patch_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_patch_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_logging_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_logging_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_logging_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_cancel_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_cancel_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_cancel_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_stream_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_stream_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_stream_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_read_stream_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_read_stream_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_read_stream_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_patch_stream_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_patch_stream_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_patch_stream_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_timeout_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_timeout_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_timeout_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_workspace_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_workspace_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_workspace_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_workspace_edges
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_workspace_edges.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_workspace_edges PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_fs_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_fs_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_fs_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_search_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_search_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_search_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_patch_lifecycle
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_patch_lifecycle.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_patch_lifecycle PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_stream_cancel_timeout
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_stream_cancel_timeout.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_stream_cancel_timeout PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_cancel_edges
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_cancel_edges.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_cancel_edges PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_logging_release
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_logging_release.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_logging_release PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_cli_contract
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_cli_contract.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_cli_contract PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_session_drop_change_contract
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_session_drop_change_contract.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_session_drop_change_contract PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_functional_session_drop_change_edges
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_session_drop_change_edges.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_session_drop_change_edges PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_session_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_session_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_session_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_recovery_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_recovery_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_recovery_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_structure_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_structure_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_structure_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_sdk_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_sdk_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_sdk_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")

  add_test(
    NAME test_integration_vscode_extension_ops
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration_vscode_extension_ops.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_integration_vscode_extension_ops PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")
endif()

add_executable(test_session_service tests_session_service.cpp)
target_link_libraries(test_session_service PRIVATE bridge_core)
add_test(NAME test_session_service COMMAND test_session_service)

add_executable(test_structure_adapters tests_structure_adapters.cpp)
target_link_libraries(test_structure_adapters PRIVATE bridge_core)
add_test(NAME test_structure_adapters COMMAND test_structure_adapters)
