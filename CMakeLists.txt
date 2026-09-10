cmake_minimum_required(VERSION 3.24)

project(Openbrowser VERSION 0.3.0 LANGUAGES C CXX)

option(OPENBROWSER_BUILD_TESTS "Build Openbrowser core tests" ON)
option(OPENBROWSER_BUILD_DESKTOP "Configure the pinned CEF desktop adapter" OFF)

add_library(openbrowser_core
    src/core/focus_queue/focus_queue.cpp
    src/core/focus_queue/focus_sprint.cpp
    src/core/capabilities/capability_policy.cpp
    src/core/navigation/address_input.cpp
    src/core/session/browser_session.cpp
    src/core/session/session_privacy_orchestrator.cpp
    src/core/session/focus_session_controller.cpp
    src/core/session/session_persistence.cpp
    src/core/session/tab_discard_policy.cpp
    src/core/workspaces/workspace_manager.cpp
    src/core/transfers/transfer_broker.cpp
    src/core/transfers/file_broker.cpp
    src/core/filters/content_filter.cpp
    src/core/history/history_manager.cpp
    src/core/bookmarks/bookmark_manager.cpp
    src/core/network/network_attribution.cpp
    src/core/commands/action_registry.cpp
    src/core/commands/command_palette.cpp
    src/core/sync/sync_port.cpp
    src/core/sync/local_filesystem_sync.cpp
    src/core/compatibility/compatibility_scenario.cpp
    src/core/compatibility/compatibility_runner.cpp
    src/core/profiles/profile_manager.cpp
    src/core/compatibility/compatibility_mitigations.cpp
    src/core/compatibility/user_agent_policy.cpp
    src/core/network/network_filter_query.cpp
    src/core/network/waterfall_timeline.cpp
    src/core/filters/adblock_rule_parser.cpp
    src/core/network/filter_decision_log.cpp     # M7.2
    src/core/storage/atomic_file_store.cpp
    src/core/storage/json_helper.cpp
    src/core/settings/settings_manager.cpp
    src/core/ui/transient_panel_state.cpp
)

add_library(Openbrowser::Core ALIAS openbrowser_core)

target_compile_features(openbrowser_core PUBLIC cxx_std_20)
target_include_directories(openbrowser_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(openbrowser_network_trace
    src/devtools/network/network_trace.cpp
    src/devtools/network/connection_diagnostics.cpp  # M7.1
    src/devtools/network/obtrace_recorder.cpp        # M7.3
)

add_library(Openbrowser::NetworkTrace ALIAS openbrowser_network_trace)

target_compile_features(openbrowser_network_trace PUBLIC cxx_std_20)
target_include_directories(openbrowser_network_trace PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(openbrowser_network_trace PUBLIC Openbrowser::Core)

if(MSVC)
    target_compile_options(openbrowser_core PRIVATE /W4 /permissive-)
    target_compile_options(openbrowser_network_trace PRIVATE /W4 /permissive-)
else()
    target_compile_options(openbrowser_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    target_compile_options(openbrowser_network_trace PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

if(OPENBROWSER_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(openbrowser_core_tests tests/core_tests.cpp)
    target_link_libraries(openbrowser_core_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_core_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_session_reentrant_tests tests/session_reentrant_tests.cpp)
    target_link_libraries(openbrowser_session_reentrant_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_session_reentrant_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_session_reentrant_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_navigation_command_tests tests/navigation_command_tests.cpp)
    target_link_libraries(openbrowser_navigation_command_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_navigation_command_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_navigation_command_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_address_input_tests tests/address_input_tests.cpp)
    target_link_libraries(openbrowser_address_input_tests PRIVATE Openbrowser::Core)
    target_compile_features(openbrowser_address_input_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_network_trace_tests tests/network_trace_tests.cpp)
    target_link_libraries(openbrowser_network_trace_tests PRIVATE Openbrowser::NetworkTrace)
    target_compile_features(openbrowser_network_trace_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_session_observer_tests tests/session_observer_tests.cpp)
    target_link_libraries(openbrowser_session_observer_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_session_observer_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_session_observer_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_focus_session_controller_tests tests/focus_session_controller_tests.cpp)
    target_link_libraries(openbrowser_focus_session_controller_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_focus_session_controller_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_focus_session_controller_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_session_persistence_tests tests/session_persistence_tests.cpp)
    target_link_libraries(openbrowser_session_persistence_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_session_persistence_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_session_persistence_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_capability_policy_tests tests/capability_policy_tests.cpp)
    target_link_libraries(openbrowser_capability_policy_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_capability_policy_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_capability_policy_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_capability_permission_tests tests/capability_permission_tests.cpp)
    target_link_libraries(openbrowser_capability_permission_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_capability_permission_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_capability_permission_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_focus_sprint_tests tests/focus_sprint_tests.cpp)
    target_link_libraries(openbrowser_focus_sprint_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_focus_sprint_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_focus_sprint_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_tab_discard_tests tests/tab_discard_tests.cpp)
    target_link_libraries(openbrowser_tab_discard_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_tab_discard_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_tab_discard_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_workspace_tests tests/workspace_tests.cpp)
    target_link_libraries(openbrowser_workspace_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_workspace_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_workspace_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_transfer_tests tests/transfer_tests.cpp)
    target_link_libraries(openbrowser_transfer_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_transfer_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_transfer_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_content_filter_tests tests/content_filter_tests.cpp)
    target_link_libraries(openbrowser_content_filter_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_content_filter_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_content_filter_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_history_bookmark_tests tests/history_bookmark_tests.cpp)
    target_link_libraries(openbrowser_history_bookmark_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_history_bookmark_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_history_bookmark_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_network_attribution_tests tests/network_attribution_tests.cpp)
    target_link_libraries(openbrowser_network_attribution_tests PRIVATE Openbrowser::NetworkTrace Openbrowser::Core)
    target_include_directories(openbrowser_network_attribution_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_network_attribution_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_file_broker_tests tests/file_broker_tests.cpp)
    target_link_libraries(openbrowser_file_broker_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_file_broker_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_file_broker_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_command_palette_tests tests/command_palette_tests.cpp)
    target_link_libraries(openbrowser_command_palette_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_command_palette_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_command_palette_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_sync_tests tests/sync_tests.cpp)
    target_link_libraries(openbrowser_sync_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_sync_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_sync_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_compatibility_tests tests/compatibility_tests.cpp)
    target_link_libraries(openbrowser_compatibility_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_compatibility_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_compatibility_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_profile_manager_tests tests/profile_manager_tests.cpp)
    target_link_libraries(openbrowser_profile_manager_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_profile_manager_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_profile_manager_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_compatibility_mitigation_tests tests/compatibility_mitigation_tests.cpp)
    target_link_libraries(openbrowser_compatibility_mitigation_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_compatibility_mitigation_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_compatibility_mitigation_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_network_filter_query_tests tests/network_filter_query_tests.cpp)
    target_link_libraries(openbrowser_network_filter_query_tests PRIVATE Openbrowser::NetworkTrace Openbrowser::Core)
    target_include_directories(openbrowser_network_filter_query_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_network_filter_query_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_waterfall_timeline_tests tests/waterfall_timeline_tests.cpp)
    target_link_libraries(openbrowser_waterfall_timeline_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_waterfall_timeline_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_waterfall_timeline_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_adblock_parser_tests tests/adblock_parser_tests.cpp)
    target_link_libraries(openbrowser_adblock_parser_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_adblock_parser_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_adblock_parser_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_command_palette_overlay_tests tests/command_palette_overlay_tests.cpp)
    target_link_libraries(openbrowser_command_palette_overlay_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_command_palette_overlay_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_command_palette_overlay_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_bookmarks_bar_tests tests/bookmarks_bar_tests.cpp)
    target_link_libraries(openbrowser_bookmarks_bar_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_bookmarks_bar_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_bookmarks_bar_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_downloads_panel_tests tests/downloads_panel_tests.cpp)
    target_link_libraries(openbrowser_downloads_panel_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_downloads_panel_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_downloads_panel_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_desktop_profile_integration_tests tests/desktop_profile_integration_tests.cpp)
    target_link_libraries(openbrowser_desktop_profile_integration_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_desktop_profile_integration_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_desktop_profile_integration_tests PRIVATE cxx_std_20)

    # M7 test targets
    add_executable(openbrowser_connection_diagnostics_tests tests/connection_diagnostics_tests.cpp)
    target_link_libraries(openbrowser_connection_diagnostics_tests PRIVATE Openbrowser::NetworkTrace)
    target_include_directories(openbrowser_connection_diagnostics_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_connection_diagnostics_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_filter_decision_log_tests tests/filter_decision_log_tests.cpp)
    target_link_libraries(openbrowser_filter_decision_log_tests PRIVATE Openbrowser::NetworkTrace Openbrowser::Core)
    target_include_directories(openbrowser_filter_decision_log_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_filter_decision_log_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_obtrace_recorder_tests tests/obtrace_recorder_tests.cpp)
    target_link_libraries(openbrowser_obtrace_recorder_tests PRIVATE Openbrowser::NetworkTrace)
    target_include_directories(openbrowser_obtrace_recorder_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_obtrace_recorder_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_network_lab_enhanced_tests tests/network_lab_enhanced_tests.cpp)
    target_link_libraries(openbrowser_network_lab_enhanced_tests PRIVATE Openbrowser::NetworkTrace Openbrowser::Core)
    target_include_directories(openbrowser_network_lab_enhanced_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_network_lab_enhanced_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_private_profile_isolation_tests tests/private_profile_isolation_tests.cpp)
    target_link_libraries(openbrowser_private_profile_isolation_tests PRIVATE Openbrowser::Core Openbrowser::NetworkTrace)
    target_include_directories(openbrowser_private_profile_isolation_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_private_profile_isolation_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_history_bookmark_persistence_tests tests/history_bookmark_persistence_tests.cpp)
    target_link_libraries(openbrowser_history_bookmark_persistence_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_history_bookmark_persistence_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_history_bookmark_persistence_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_browser_core_flow_tests tests/browser_core_flow_tests.cpp)
    target_link_libraries(openbrowser_browser_core_flow_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_browser_core_flow_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_browser_core_flow_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_local_persistence_tests tests/local_persistence_tests.cpp)
    target_link_libraries(openbrowser_local_persistence_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_local_persistence_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_local_persistence_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_transient_panel_state_tests tests/transient_panel_state_tests.cpp)
    target_link_libraries(openbrowser_transient_panel_state_tests PRIVATE Openbrowser::Core)
    target_include_directories(openbrowser_transient_panel_state_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(openbrowser_transient_panel_state_tests PRIVATE cxx_std_20)

    add_executable(openbrowser_ui_icon_invariants_tests tests/ui_icon_invariants_tests.cpp)
    target_include_directories(openbrowser_ui_icon_invariants_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(openbrowser_ui_icon_invariants_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(openbrowser_core_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_session_reentrant_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_navigation_command_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_address_input_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_network_trace_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_session_observer_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_focus_session_controller_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_session_persistence_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_capability_policy_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_capability_permission_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_focus_sprint_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_tab_discard_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_workspace_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_transfer_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_content_filter_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_history_bookmark_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_network_attribution_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_file_broker_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_command_palette_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_sync_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_compatibility_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_profile_manager_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_compatibility_mitigation_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_network_filter_query_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_waterfall_timeline_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_adblock_parser_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_command_palette_overlay_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_bookmarks_bar_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_downloads_panel_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_desktop_profile_integration_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_connection_diagnostics_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_filter_decision_log_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_obtrace_recorder_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_network_lab_enhanced_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_browser_core_flow_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_local_persistence_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_transient_panel_state_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_ui_icon_invariants_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(openbrowser_core_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_session_reentrant_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_navigation_command_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_address_input_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_network_trace_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_session_observer_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_focus_session_controller_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_session_persistence_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_capability_policy_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_capability_permission_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_focus_sprint_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_tab_discard_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_workspace_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_transfer_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_content_filter_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_history_bookmark_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_network_attribution_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_file_broker_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_command_palette_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_sync_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_compatibility_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_profile_manager_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_compatibility_mitigation_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_network_filter_query_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_waterfall_timeline_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_adblock_parser_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_command_palette_overlay_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_bookmarks_bar_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_downloads_panel_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_desktop_profile_integration_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_connection_diagnostics_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_filter_decision_log_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_obtrace_recorder_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_network_lab_enhanced_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_browser_core_flow_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_local_persistence_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_transient_panel_state_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_ui_icon_invariants_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    endif()

    add_test(NAME openbrowser.core COMMAND openbrowser_core_tests)
    add_test(NAME openbrowser.session_reentrant COMMAND openbrowser_session_reentrant_tests)
    add_test(NAME openbrowser.navigation_commands COMMAND openbrowser_navigation_command_tests)
    add_test(NAME openbrowser.address_input COMMAND openbrowser_address_input_tests)
    add_test(NAME openbrowser.network_trace COMMAND openbrowser_network_trace_tests)
    add_test(NAME openbrowser.session_observer COMMAND openbrowser_session_observer_tests)
    add_test(NAME openbrowser.focus_session_controller COMMAND openbrowser_focus_session_controller_tests)
    add_test(NAME openbrowser.session_persistence COMMAND openbrowser_session_persistence_tests)
    add_test(NAME openbrowser.capability_policy COMMAND openbrowser_capability_policy_tests)
    add_test(NAME openbrowser.capability_permission COMMAND openbrowser_capability_permission_tests)
    add_test(NAME openbrowser.focus_sprint COMMAND openbrowser_focus_sprint_tests)
    add_test(NAME openbrowser.tab_discard COMMAND openbrowser_tab_discard_tests)
    add_test(NAME openbrowser.workspace COMMAND openbrowser_workspace_tests)
    add_test(NAME openbrowser.transfers COMMAND openbrowser_transfer_tests)
    add_test(NAME openbrowser.content_filter COMMAND openbrowser_content_filter_tests)
    add_test(NAME openbrowser.history_bookmark COMMAND openbrowser_history_bookmark_tests)
    add_test(NAME openbrowser.network_attribution COMMAND openbrowser_network_attribution_tests)
    add_test(NAME openbrowser.file_broker COMMAND openbrowser_file_broker_tests)
    add_test(NAME openbrowser.command_palette COMMAND openbrowser_command_palette_tests)
    add_test(NAME openbrowser.sync COMMAND openbrowser_sync_tests)
    add_test(NAME openbrowser.compatibility COMMAND openbrowser_compatibility_tests)
    add_test(NAME openbrowser.profile_manager COMMAND openbrowser_profile_manager_tests)
    add_test(NAME openbrowser.compatibility_mitigations COMMAND openbrowser_compatibility_mitigation_tests)
    add_test(NAME openbrowser.network_filter_query COMMAND openbrowser_network_filter_query_tests)
    add_test(NAME openbrowser.waterfall_timeline COMMAND openbrowser_waterfall_timeline_tests)
    add_test(NAME openbrowser.adblock_parser COMMAND openbrowser_adblock_parser_tests)
    add_test(NAME openbrowser.command_palette_overlay COMMAND openbrowser_command_palette_overlay_tests)
    add_test(NAME openbrowser.bookmarks_bar COMMAND openbrowser_bookmarks_bar_tests)
    add_test(NAME openbrowser.downloads_panel COMMAND openbrowser_downloads_panel_tests)
    add_test(NAME openbrowser.desktop_profile_integration COMMAND openbrowser_desktop_profile_integration_tests)
    # M7 tests
    add_test(NAME openbrowser.connection_diagnostics COMMAND openbrowser_connection_diagnostics_tests)
    add_test(NAME openbrowser.filter_decision_log COMMAND openbrowser_filter_decision_log_tests)
    add_test(NAME openbrowser.obtrace_recorder COMMAND openbrowser_obtrace_recorder_tests)
    add_test(NAME openbrowser.network_lab_enhanced COMMAND openbrowser_network_lab_enhanced_tests)
    add_test(NAME openbrowser.private_profile_isolation COMMAND openbrowser_private_profile_isolation_tests)
    add_test(NAME openbrowser.history_bookmark_persistence COMMAND openbrowser_history_bookmark_persistence_tests)
    # MVP-1 tests
    add_test(NAME openbrowser.browser_core_flow COMMAND openbrowser_browser_core_flow_tests)
    # MVP-2 tests
    add_test(NAME openbrowser.local_persistence COMMAND openbrowser_local_persistence_tests)
    add_test(NAME openbrowser.transient_panel_state COMMAND openbrowser_transient_panel_state_tests)
    add_test(NAME openbrowser.ui_icon_invariants COMMAND openbrowser_ui_icon_invariants_tests)

    # M8 compatibility harness protocol and local-fixture integration test.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME openbrowser.compatibility_harness
            COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/compatibility/test_compatibility_harness.py")
        set_tests_properties(
            openbrowser.compatibility_harness
            PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endif()



if(OPENBROWSER_BUILD_DESKTOP)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenbrowserCEF.cmake")
    openbrowser_configure_cef()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/apps/desktop/CMakeLists.txt")
        add_subdirectory(apps/desktop)
    else()
        message(STATUS "CEF bootstrap validated; apps/desktop target has not been added yet")
    endif()
endif()
