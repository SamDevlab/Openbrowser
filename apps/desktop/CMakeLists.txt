set(OPENBROWSER_DESKTOP_TARGET "openbrowser")

set(OPENBROWSER_DESKTOP_COMMON_SOURCES
    browser_chrome.cpp
    browser_chrome.h
    cef_browser_engine.cpp
    cef_browser_engine.h
    cef_tab_client.cpp
    cef_tab_client.h
    desktop_app.cpp
    desktop_app.h
)

if(OS_LINUX)
    set(OPENBROWSER_DESKTOP_SOURCES
        ${OPENBROWSER_DESKTOP_COMMON_SOURCES}
        main_linux.cpp
    )
elseif(OS_WINDOWS)
    set(OPENBROWSER_DESKTOP_SOURCES
        ${OPENBROWSER_DESKTOP_COMMON_SOURCES}
        main_win.cpp
    )
elseif(OS_MAC)
    message(FATAL_ERROR
        "The M1 CEF runtime target is not enabled on macOS yet. "
        "The core remains macOS-tested; a correct CEF helper-app bundle will be added before desktop support is declared."
    )
else()
    message(FATAL_ERROR "Unsupported desktop platform for the CEF adapter")
endif()

ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
SET_CEF_TARGET_OUT_DIR()

if(OS_LINUX)
    add_executable(${OPENBROWSER_DESKTOP_TARGET} ${OPENBROWSER_DESKTOP_SOURCES})
    SET_EXECUTABLE_TARGET_PROPERTIES(${OPENBROWSER_DESKTOP_TARGET})

    add_dependencies(${OPENBROWSER_DESKTOP_TARGET} libcef_dll_wrapper)
    target_link_libraries(
        ${OPENBROWSER_DESKTOP_TARGET}
        PRIVATE
            Openbrowser::Core
            Openbrowser::NetworkTrace
            libcef_lib
            libcef_dll_wrapper
            ${CEF_STANDARD_LIBS}
    )

    set_target_properties(${OPENBROWSER_DESKTOP_TARGET} PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
        RUNTIME_OUTPUT_DIRECTORY "${CEF_TARGET_OUT_DIR}"
    )

    COPY_FILES(
        "${OPENBROWSER_DESKTOP_TARGET}"
        "${CEF_BINARY_FILES}"
        "${CEF_BINARY_DIR}"
        "${CEF_TARGET_OUT_DIR}"
    )
    COPY_FILES(
        "${OPENBROWSER_DESKTOP_TARGET}"
        "${CEF_RESOURCE_FILES}"
        "${CEF_RESOURCE_DIR}"
        "${CEF_TARGET_OUT_DIR}"
    )

    if(EXISTS "${CEF_BINARY_DIR}/libminigbm.so")
        COPY_FILES(
            "${OPENBROWSER_DESKTOP_TARGET}"
            "libminigbm.so"
            "${CEF_BINARY_DIR}"
            "${CEF_TARGET_OUT_DIR}"
        )
    endif()

    if(USE_SANDBOX)
        SET_LINUX_SUID_PERMISSIONS(
            "${OPENBROWSER_DESKTOP_TARGET}"
            "${CEF_TARGET_OUT_DIR}/chrome-sandbox"
        )
    endif()
endif()

if(OS_WINDOWS)
    if(USE_SANDBOX)
        # Current CEF Windows sandboxing uses bootstrap.exe to load the client
        # DLL. This follows the upstream cefsimple deployment model rather than
        # silently disabling the sandbox.
        add_library(${OPENBROWSER_DESKTOP_TARGET} SHARED ${OPENBROWSER_DESKTOP_SOURCES})
        SET_LIBRARY_TARGET_PROPERTIES(${OPENBROWSER_DESKTOP_TARGET})
        COPY_SINGLE_FILE(
            ${OPENBROWSER_DESKTOP_TARGET}
            "${CEF_BINARY_DIR}/bootstrap.exe"
            "${CEF_TARGET_OUT_DIR}/${OPENBROWSER_DESKTOP_TARGET}.exe"
        )

        if(CMAKE_GENERATOR MATCHES "Visual Studio")
            set_target_properties(${OPENBROWSER_DESKTOP_TARGET} PROPERTIES
                VS_DEBUGGER_COMMAND "$<TARGET_FILE_DIR:${OPENBROWSER_DESKTOP_TARGET}>/${OPENBROWSER_DESKTOP_TARGET}.exe"
            )
        endif()
    else()
        add_executable(${OPENBROWSER_DESKTOP_TARGET} WIN32 ${OPENBROWSER_DESKTOP_SOURCES})
        SET_EXECUTABLE_TARGET_PROPERTIES(${OPENBROWSER_DESKTOP_TARGET})
    endif()

    add_dependencies(${OPENBROWSER_DESKTOP_TARGET} libcef_dll_wrapper)
    target_link_libraries(
        ${OPENBROWSER_DESKTOP_TARGET}
        PRIVATE
            Openbrowser::Core
            Openbrowser::NetworkTrace
            libcef_lib
            libcef_dll_wrapper
            ${CEF_STANDARD_LIBS}
    )

    COPY_FILES(
        "${OPENBROWSER_DESKTOP_TARGET}"
        "${CEF_BINARY_FILES}"
        "${CEF_BINARY_DIR}"
        "${CEF_TARGET_OUT_DIR}"
    )
    COPY_FILES(
        "${OPENBROWSER_DESKTOP_TARGET}"
        "${CEF_RESOURCE_FILES}"
        "${CEF_RESOURCE_DIR}"
        "${CEF_TARGET_OUT_DIR}"
    )

    if(USE_SANDBOX)
        SET_LPAC_ACLS("${OPENBROWSER_DESKTOP_TARGET}")
    endif()
endif()

target_include_directories(
    ${OPENBROWSER_DESKTOP_TARGET}
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CEF_ROOT}"
)
target_compile_features(${OPENBROWSER_DESKTOP_TARGET} PRIVATE cxx_std_20)
