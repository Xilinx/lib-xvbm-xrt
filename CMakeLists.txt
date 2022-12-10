# SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

# Copyright (C) 2019-2021 Xilinx Inc - All rights reserved
# Xilinx Video Buffer Manager (Xvbm)
#
# This file is dual-licensed; you may select either the GNU
# Lesser General Public License version 3 or
# Apache License, Version 2.0.

cmake_minimum_required(VERSION 2.8.11)

set(XVBM_PROJ "xilinx-xvbm")
set(XVBM_LIB_NAME "xvbm")

find_program(UNAME_EXEC uname)
find_program(LSB_RELEASE_EXEC lsb_release)

execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    OUTPUT_VARIABLE LINUX_FLAVOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (${LINUX_FLAVOR} MATCHES "^(Ubuntu|Debian)")
        # amd64
        execute_process(
        COMMAND dpkg --print-architecture
        OUTPUT_VARIABLE CPACK_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
elseif (${LINUX_FLAVOR} MATCHES "^(RedHat|CentOS|Amazon|Fedora|SUSE)")
        # x86_64
        execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE CPACK_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
else()
        message( SEND_ERROR "Trying to build on incompatible system \"${LINUX_FLAVOR}\"." )
endif()

# Grab OS release short name
execute_process(COMMAND ${LSB_RELEASE_EXEC} -rs
        OUTPUT_VARIABLE LSB_RELEASE_REL_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE)

if (${LINUX_FLAVOR} MATCHES "^Amazon")
        set(LSB_RELEASE_REL_SHORT "amzn${LSB_RELEASE_REL_SHORT}")
endif()

project(${XVBM_PROJ})

# Packaging section
set(CPACK_PACKAGE_NAME ${XVBM_PROJ})
set(CPACK_PACKAGE_VERSION "1.0.3")
set(CPACK_PACKAGE_FILE_NAME "${XVBM_PROJ}_${CPACK_PACKAGE_VERSION}_${LSB_RELEASE_REL_SHORT}-${CPACK_ARCH}")
set(CPACK_PACKAGE_CONTACT "John Nichols <jnichol@xilinx.com>")
set(CPACK_PACKAGE_VENDOR "Xilinx")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Xilinx Video Buffer Manager (XVBM)")
set(CPACK_PACKAGE_DESCRIPTION "The XVBM manages shared device buffers
                               among kernels for zero-copy")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "xrt (>= 2.11)")
set(CPACK_RPM_PACKAGE_REQUIRES "xrt >= 2.11")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/opt" "/opt/xilinx" "/usr/lib" "/usr/lib/pkgconfig" "/usr/share/doc/xilinx" "/usr/share/doc/xilinx/u30")
include(CPack)

set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/lib/pkgconfig")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_FILE_LICENSE_PATH "/usr/share/doc/amd/u30")

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    include(CodeCoverage)
    append_coverage_compiler_flags()
    set(COVERAGE_LCOV_EXCLUDES '/usr/include/*'
                               '/opt/xilinx/xrt/include/*'
                               'tests/*')
    setup_target_for_coverage_lcov(NAME coverage
                                   EXECUTABLE ctest
                                   DEPENDENCIES unit_tests
                                   LCOV_ARGS --rc lcov_branch_coverage=1
                                   GENHTML_ARGS --branch-coverage)
endif()

# Linking variables
find_package(PkgConfig REQUIRED)
pkg_check_modules(XRT REQUIRED xrt)

# However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "lib/*.cpp")

# Bring the headers into the project
include_directories(include)

# Generate the shared library from the sources
add_library(${XVBM_LIB_NAME} STATIC ${SOURCES})

target_compile_options(${XVBM_LIB_NAME} PUBLIC ${XRT_CFLAGS})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    enable_testing()
    add_subdirectory(tests)
endif()


# Set the location for library installation
install(TARGETS ${XVBM_LIB_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include
        FILES_MATCHING PATTERN "*.h")
install(FILES ${CPACK_RESOURCE_FILE_LICENSE} DESTINATION ${CPACK_FILE_LICENSE_PATH}/xvbm)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    configure_file(${CMAKE_SOURCE_DIR}/lib/xvbm.pc.in
                   ${CMAKE_SOURCE_DIR}/lib/xvbm.pc @ONLY)
    set(PKGCFG_FILE "lib/xvbm.pc")
    install(FILES ${CMAKE_SOURCE_DIR}/lib/xvbm.pc
            DESTINATION ${XVBM_PC_INSTALL_PATH})
endif()
