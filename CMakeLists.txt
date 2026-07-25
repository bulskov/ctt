cmake_minimum_required(VERSION 3.14)  # FetchContent_MakeAvailable

project(ctt
    VERSION 0.1.0
    DESCRIPTION "A tiny single-header C unit-test library"
    HOMEPAGE_URL "https://github.com/bulskov/ctt"
    LANGUAGES C)

# Are we the top-level project, or being pulled in via add_subdirectory/
# FetchContent? Examples and self-tests are only built at top level.
if(DEFINED PROJECT_IS_TOP_LEVEL)
    set(CTT_TOP_LEVEL ${PROJECT_IS_TOP_LEVEL})
else()
    set(CTT_TOP_LEVEL OFF)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(CTT_TOP_LEVEL ON)
    endif()
endif()

option(CTT_BUILD_EXAMPLES "Build ctt examples" ${CTT_TOP_LEVEL})
option(CTT_BUILD_TESTS "Build ctt's own test suite" ${CTT_TOP_LEVEL})
option(CTT_INSTALL "Generate install rules for ctt" ${CTT_TOP_LEVEL})

include(GNUInstallDirs)  # define CMAKE_INSTALL_* before they are used below

# --- Library ---------------------------------------------------------------
# Static library built from the stub TU. The public header lives in include/.
# Consumers link ctt::ctt and #include "ctt.h" — no CTT_IMPLEMENTATION needed.
add_library(ctt STATIC src/ctt.c)
add_library(ctt::ctt ALIAS ctt)

target_include_directories(ctt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(ctt PUBLIC c_std_11)

# --- Examples + tests ------------------------------------------------------
if(CTT_BUILD_EXAMPLES OR CTT_BUILD_TESTS)
    enable_testing()
endif()

if(CTT_BUILD_EXAMPLES)
    add_executable(ctt_example examples/example_tests.c)
    target_link_libraries(ctt_example PRIVATE ctt::ctt)
    add_test(NAME ctt_example COMMAND ctt_example)
endif()

if(CTT_BUILD_TESTS)
    # Positive paths: ctt asserting on itself.
    add_executable(ctt_self_tests tests/self_tests.c)
    target_link_libraries(ctt_self_tests PRIVATE ctt::ctt)
    add_test(NAME ctt_self_tests COMMAND ctt_self_tests)

    # Negative/crash paths: a shell driver that compiles crafted suites and
    # inspects their output + exit codes. POSIX shell only.
    if(NOT WIN32)
        add_test(NAME ctt_behavior
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior_tests.sh
                    ${CMAKE_C_COMPILER} ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
endif()

# --- Install / find_package(ctt) ------------------------------------------
if(CTT_INSTALL)
    include(CMakePackageConfigHelpers)

    install(TARGETS ctt
        EXPORT cttTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT cttTargets
        FILE cttTargets.cmake
        NAMESPACE ctt::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ctt)

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cttConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cttConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ctt)

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/cttConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/cttConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cttConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ctt)
endif()
