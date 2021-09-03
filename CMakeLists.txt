# SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

# Copyright (C) 2019-2021 Xilinx Inc - All rights reserved
# Xilinx Video Buffer Manager (Xvbm)
#
# This file is dual-licensed; you may select either the GNU
# Lesser General Public License version 3 or
# Apache License, Version 2.0.

cmake_minimum_required(VERSION 2.8.11)

project(xvbm) 
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/lib/pkgconfig")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)

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
add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_compile_options(${PROJECT_NAME} PUBLIC ${XRT_CFLAGS})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    enable_testing()
    add_subdirectory(tests)
endif()


# Set the location for library installation
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include
        FILES_MATCHING PATTERN "*.h")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    configure_file(${CMAKE_SOURCE_DIR}/lib/xvbm.pc.in
                   ${CMAKE_SOURCE_DIR}/lib/xvbm.pc @ONLY)
    set(PKGCFG_FILE "lib/xvbm.pc")
    install(FILES ${CMAKE_SOURCE_DIR}/lib/xvbm.pc 
            DESTINATION ${XVBM_PC_INSTALL_PATH})
endif()

# Packaging section
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "1.0.1")
set(CPACK_PACKAGE_CONTACT "John Nichols <jnichol@xilinx.com>")
set(CPACK_PACKAGE_VENDOR "Xilinx")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Xilinx Video Buffer Manager (XVBM)")
set(CPACK_PACKAGE_DESCRIPTION "The XVBM manages shared device buffers 
                               among kernels for zero-copy") 

set(CPACK_DEBIAN_PACKAGE_DEPENDS "xrt (>= 2.10.37)")
set(CPACK_RPM_PACKAGE_DEPENDS "xrt >= 2.10.37")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/opt" "/opt/xilinx" "/usr/lib" "/usr/lib/pkgconfig")
include(CPack)
