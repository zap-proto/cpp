# Copyright (C) 2026, Lux Industries Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause-Eco
#
# The ZAP C++ SDK: the message layer, the call envelope with promise
# pipelining, and the symmetric transport that carries them.
#
# Consumers link zap::zap. It is installable and exported, so a downstream
# project reaches it with find_package(Zap) against an install, or with
# FetchContent against a tag — never by including a path into this tree.

cmake_minimum_required(VERSION 3.20)
project(Zap VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Tests are for this project's own build; a consumer that pulls zap in as a
# subproject gets the library alone.
option(ZAP_BUILD_TESTS "Build the ZAP C++ test suite" ${PROJECT_IS_TOP_LEVEL})

find_package(Threads REQUIRED)

add_library(zap STATIC
  src/rpc.cpp
  src/transport.cpp
)
add_library(zap::zap ALIAS zap)

target_include_directories(zap PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(zap PUBLIC cxx_std_23)
target_link_libraries(zap PUBLIC Threads::Threads)
target_compile_options(zap PRIVATE -Wall -Wextra)

# ── tests ───────────────────────────────────────────────────────────────────
#   codec       the message layer, against bytes the canonical runtime printed
#   wire        the canonical runtime's own zap_test.go, case for case
#   rpc         the call envelope and the promise table
#   transport   the protocol: both peers initiate, many calls in flight,
#               answers out of order
if(ZAP_BUILD_TESTS)
  enable_testing()
  foreach(_t codec wire rpc transport)
    add_executable(zap_${_t}_test test/${_t}_test.cpp)
    target_link_libraries(zap_${_t}_test PRIVATE zap)
    target_compile_options(zap_${_t}_test PRIVATE -Wall -Wextra)
    add_test(NAME ${_t} COMMAND zap_${_t}_test)
  endforeach()
endif()

# ── install / export ────────────────────────────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS zap EXPORT ZapTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT ZapTargets NAMESPACE zap:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Zap)

configure_package_config_file(cmake/ZapConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ZapConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Zap)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ZapConfigVersion.cmake
  COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ZapConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/ZapConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Zap)
