cmake_minimum_required(VERSION 3.24)

project(Openbrowser VERSION 0.1.0 LANGUAGES C CXX)

option(OPENBROWSER_BUILD_TESTS "Build Openbrowser core tests" ON)
option(OPENBROWSER_BUILD_DESKTOP "Configure the pinned CEF desktop adapter" OFF)

add_library(openbrowser_core
    src/core/focus_queue/focus_queue.cpp
    src/core/capabilities/capability_policy.cpp
    src/core/navigation/address_input.cpp
    src/core/session/browser_session.cpp
    src/core/session/focus_session_controller.cpp
    src/core/session/session_persistence.cpp
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
