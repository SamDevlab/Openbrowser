# SPDX-License-Identifier: MPL-2.0

set(
    OPENBROWSER_CEF_VERSION
    "152.0.6+g708dc14+chromium-152.0.7977.83"
    CACHE STRING
    "Exact CEF binary distribution version supported by the current Openbrowser desktop bootstrap"
)

macro(openbrowser_configure_cef)
    if(NOT CEF_ROOT AND DEFINED ENV{CEF_ROOT})
        set(CEF_ROOT "$ENV{CEF_ROOT}")
    endif()

    if(NOT CEF_ROOT)
        message(FATAL_ERROR
            "OPENBROWSER_BUILD_DESKTOP=ON requires CEF_ROOT to point to the exact "
            "CEF ${OPENBROWSER_CEF_VERSION} standard binary distribution. "
            "The default Openbrowser build never downloads CEF automatically."
        )
    endif()

    get_filename_component(CEF_ROOT "${CEF_ROOT}" ABSOLUTE)
    set(_openbrowser_cef_version_header "${CEF_ROOT}/include/cef_version.h")

    if(NOT EXISTS "${_openbrowser_cef_version_header}")
        message(FATAL_ERROR
            "CEF_ROOT='${CEF_ROOT}' does not contain include/cef_version.h. "
            "Use an extracted CEF standard binary distribution."
        )
    endif()

    file(READ "${_openbrowser_cef_version_header}" _openbrowser_cef_version_header_contents)
    string(
        FIND
        "${_openbrowser_cef_version_header_contents}"
        "#define CEF_VERSION \"${OPENBROWSER_CEF_VERSION}\""
        _openbrowser_cef_version_match
    )

    if(_openbrowser_cef_version_match EQUAL -1)
        string(REGEX MATCH
            "#define CEF_VERSION \"([^\"]+)\""
            _openbrowser_cef_detected_line
            "${_openbrowser_cef_version_header_contents}"
        )
        set(_openbrowser_cef_detected_version "unknown")
        if(CMAKE_MATCH_1)
            set(_openbrowser_cef_detected_version "${CMAKE_MATCH_1}")
        endif()

        message(FATAL_ERROR
            "Unsupported CEF distribution at '${CEF_ROOT}'. "
            "Expected ${OPENBROWSER_CEF_VERSION}, detected ${_openbrowser_cef_detected_version}. "
            "CEF upgrades are deliberate architecture changes and must update the pin after validation."
        )
    endif()

    if(NOT EXISTS "${CEF_ROOT}/LICENSE.txt" OR NOT EXISTS "${CEF_ROOT}/CREDITS.html")
        message(FATAL_ERROR
            "CEF distribution is missing LICENSE.txt or CREDITS.html. "
            "Openbrowser will not build a redistributable desktop shell without upstream notices."
        )
    endif()

    list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
    find_package(CEF REQUIRED)

    if(NOT CEF_LIBCEF_DLL_WRAPPER_PATH)
        message(FATAL_ERROR "CEF configuration did not expose CEF_LIBCEF_DLL_WRAPPER_PATH")
    endif()

    add_subdirectory(
        "${CEF_LIBCEF_DLL_WRAPPER_PATH}"
        "${CMAKE_BINARY_DIR}/third_party/cef/libcef_dll_wrapper"
    )

    message(STATUS "Openbrowser desktop CEF: ${OPENBROWSER_CEF_VERSION}")
endmacro()
