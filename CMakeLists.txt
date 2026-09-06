cmake_minimum_required(VERSION 3.24)

project(Openbrowser VERSION 0.0.1 LANGUAGES CXX)

option(OPENBROWSER_BUILD_TESTS "Build Openbrowser core tests" ON)

add_library(openbrowser_core
    src/core/focus_queue/focus_queue.cpp
    src/core/capabilities/capability_policy.cpp
)

add_library(Openbrowser::Core ALIAS openbrowser_core)

target_compile_features(openbrowser_core PUBLIC cxx_std_20)
target_include_directories(openbrowser_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(MSVC)
    target_compile_options(openbrowser_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(openbrowser_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(OPENBROWSER_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(openbrowser_core_tests tests/core_tests.cpp)
    target_link_libraries(openbrowser_core_tests PRIVATE Openbrowser::Core)
    target_compile_features(openbrowser_core_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(openbrowser_core_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(openbrowser_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME openbrowser.core COMMAND openbrowser_core_tests)
endif()
