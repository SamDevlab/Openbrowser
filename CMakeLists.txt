cmake_minimum_required(VERSION 3.24)

project(Openbrowser VERSION 0.1.0 LANGUAGES C CXX)

option(OPENBROWSER_BUILD_TESTS "Build Openbrowser core tests" ON)
option(OPENBROWSER_BUILD_DESKTOP "Configure the pinned CEF desktop adapter" OFF)

add_library(openbrowser_core
    src/core/focus_queue/focus_queue.cpp
    src/core/focus_queue/focus_sprint.cpp
    src/core/capabilities/capability_policy.cpp
    src/core/navigation/address_input.cpp
    src/core/session/browser_session.cpp
    src/core/session/focus_session_controller.cpp
    src/core/session/session_persistence.cpp
    src/core/session/tab_discard_policy.cpp
    src/core/workspaces/workspace_manager.cpp
    src/core/transfers/transfer_broker.cpp
    src/core/filters/content_filter.cpp
    src/core/history/history_manager.cpp
    src/core/bookmarks/bookmark_manager.cpp
    src/core/network/network_attribution.cpp
)

add_library(Openbrowser::Core ALIAS openbrowser_core)

target_compile_features(openbrowser_core PUBLIC cxx_std_20)
target_include_directories(openbrowser_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(openbrowser_network_trace
    src/devtools/network/network_trace.cpp
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
