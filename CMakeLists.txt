# mod_dup - duplicates apache requests
#
# Copyright (C) 2013 Orange
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project(mod_dup)

cmake_minimum_required (VERSION 2.6)

# The version number.
set (${CMAKE_PROJECT_NAME}_VERSION_MAJOR 2)
set (${CMAKE_PROJECT_NAME}_VERSION_MINOR 0)
set (${CMAKE_PROJECT_NAME}_VERSION_PATCH 0)
set(${CMAKE_PROJECT_NAME}_VERSION ${${CMAKE_PROJECT_NAME}_VERSION_MAJOR}.${${CMAKE_PROJECT_NAME}_VERSION_MINOR}.${${CMAKE_PROJECT_NAME}_VERSION_PATCH})
set(PROJECT_VERSION ${${CMAKE_PROJECT_NAME}_VERSION})

#-----------------------------------------------------------------------------
# CMake variables
#-----------------------------------------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})
set(BUILD_UNIT_TESTS OFF CACHE BOOL "Build unit tests")
set(BUILD_COVERAGE OFF CACHE BOOL "Build with coverage testing")
set(INPUT_CMAKE_DIR ${CMAKE_SOURCE_DIR}/cmake)
set(OUTPUT_CMAKE_DIR ${CMAKE_BINARY_DIR}/cmake)


#-----------------------------------------------------------------------------
# Set some compiler options
#-----------------------------------------------------------------------------

if(DEBUG)
	set(CMAKE_CXX_FLAGS "-g -O2 -Wall -std=c++0x")
else()
	set(CMAKE_CXX_FLAGS "-O2 -Wall -std=c++0x")
endif()

if(BUILD_COVERAGE)
    set(CMAKE_CXX_FLAGS "-ggdb --coverage -O0 -Wall -fno-inline -fno-elide-constructors -fno-default-inline -std=c++0x")
    set(BUILD_UNIT_TESTS ON)
    add_definitions(-DCOVERAGE)
    add_definitions(-DDEBUG)
else()
  if(BUILD_UNIT_TESTS)
    set(CMAKE_CXX_FLAGS "-g -O0 -fno-inline -Wall -Wno-deprecated -std=c++0x")
    add_definitions(-DDEBUG)
  else()
    add_definitions(-DNDEBUG)
  endif()
endif()

if(BUILD_UNIT_TESTS)
    enable_testing()

    #-----------------------------------------------------------------------------
    # Package dependencies
    #-----------------------------------------------------------------------------
    # find_package(cppunit REQUIRED)
    #-----------------------------------------------------------------------------
    # check target
    #-------------------------------------------------------------------------------
    add_custom_target(check make -j4 all
      COMMAND make test)
endif()

if(BUILD_COVERAGE AND BUILD_UNIT_TESTS)
    #-----------------------------------------------------------------------------
    # coverage target
    #-----------------------------------------------------------------------------
    set(COVERDIR ${CMAKE_BINARY_DIR}/measures/coverage CACHE STRING "Path to the coverage results" FORCE)
    set(COVERAGE_TOP_DIR ${CMAKE_BINARY_DIR})
    set(TEMP_DIR ${CMAKE_BINARY_DIR}/temp)
    set(COVERAGE_INFO_FILE ${COVERDIR}/all.info)
    set(COVERAGE_HTML_DIR ${COVERDIR}/html CACHE STRING "genhtml output directory" FORCE)
    set(COVERAGE_HISTORY_PATH ${COVERDIR}/all.history.csv CACHE STRING "Path to the coverage history file" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} --coverage)
    target_link_libraries(gcov)

    # Run the binary for coverage of the exception in AccessLogParser in case of redirection instead of pipe
    add_custom_target(coverage
      COMMAND make coverage-clean
      COMMAND make -j2 all
      COMMAND make test
      COMMAND make coverage-test)

    add_custom_target(coverage-campaign
      COMMAND make coverage)

    #-----------------------------------------------------------------------------
    # coverage-test target
    #-----------------------------------------------------------------------------
    configure_file(${INPUT_CMAKE_DIR}/coverage-test.cmake.in ${OUTPUT_CMAKE_DIR}/coverage-test.cmake IMMEDIATE @ONLY)
    add_custom_target(coverage-test
        COMMAND "${CMAKE_COMMAND}" -P ${OUTPUT_CMAKE_DIR}/coverage-test.cmake)

    #-----------------------------------------------------------------------------
    # coverage-clean target
    #-----------------------------------------------------------------------------
    configure_file(${INPUT_CMAKE_DIR}/coverage-clean.cmake.in ${OUTPUT_CMAKE_DIR}/coverage-clean.cmake IMMEDIATE @ONLY)
    add_custom_target(coverage-clean
        COMMAND "${CMAKE_COMMAND}" -P ${OUTPUT_CMAKE_DIR}/coverage-clean.cmake)

endif()


if(DEBUG)
  add_definitions(-DDEBUG)
endif()

# add the binary tree to the search path for include files
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")

add_subdirectory(src)

# Unit tests
if(BUILD_UNIT_TESTS)
  ENABLE_TESTING()
  add_subdirectory(unit)
endif()

add_subdirectory(docs)
