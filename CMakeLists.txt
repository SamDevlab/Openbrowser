cmake_minimum_required(VERSION 3.24)

project(Openbrowser VERSION 0.1.0 LANGUAGES CXX)

option(OPENBROWSER_BUILD_TESTS "Build Openbrowser core tests" ON)
option(OPENBROWSER_BUILD_DESKTOP "Configure the pinned CEF desktop adapter" OFF)

add_library(openbrowser_core
    src/core/focus_queue/focus_queue.cpp
    src/core/capabilities/capability_policy.cpp
    src/core/session/browser_session.cpp
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

    add_executable(openbrowser_network_trace_tests tests/network_trace_tests.cpp)
    target_link_libraries(openbrowser_network_trace_tests PRIVATE Openbrowser::NetworkTrace)
    target_compile_features(openbrowser_network_trace_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(openbrowser_core_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_session_reentrant_tests PRIVATE /W4 /permissive-)
        target_compile_options(openbrowser_network_trace_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(openbrowser_core_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_session_reentrant_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        target_compile_options(openbrowser_network_trace_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    endif()

    add_test(NAME openbrowser.core COMMAND openbrowser_core_tests)
    add_test(NAME openbrowser.session_reentrant COMMAND openbrowser_session_reentrant_tests)
    add_test(NAME openbrowser.network_trace COMMAND openbrowser_network_trace_tests)
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
