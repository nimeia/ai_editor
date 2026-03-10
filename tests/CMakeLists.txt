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

add_test(NAME test_bridge_cli_version COMMAND $<TARGET_FILE:bridge_cli> --version)
add_test(NAME test_bridge_daemon_version COMMAND $<TARGET_FILE:bridge_daemon> --version)

if(WIN32)
  add_test(
    NAME test_functional_windows_native
    COMMAND pwsh -File ${CMAKE_CURRENT_SOURCE_DIR}/functional_windows_native.ps1 $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_windows_native PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")
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
    NAME test_functional_logging_release
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/functional_logging_release.sh $<TARGET_FILE:bridge_daemon> $<TARGET_FILE:bridge_cli> ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(test_functional_logging_release PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")
endif()
