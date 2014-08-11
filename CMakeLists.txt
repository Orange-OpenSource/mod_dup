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
set(BUILD_TOOLS ON CACHE BOOL "Build mod_dup tools")
set(INPUT_CMAKE_DIR ${CMAKE_SOURCE_DIR}/cmake)
set(OUTPUT_CMAKE_DIR ${CMAKE_BINARY_DIR}/cmake)

#-----------------------------------------------------------------------------
# Set some compiler options
#-----------------------------------------------------------------------------

if(DEBUG)
  set(CMAKE_CXX_FLAGS "-g -O2 -Wall -std=c++0x -fPIC")
else()
  set(CMAKE_CXX_FLAGS "-O2 -Wall -std=c++0x -fPIC")
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
#    add_definitions(-DNDEBUG)
  endif()
endif()

## Unit test rule definition
if(BUILD_UNIT_TESTS)
  enable_testing()
  add_custom_target(check make -j4 all
    COMMAND make test ARGS='-V')
endif()

if(BUILD_COVERAGE)
    #-----------------------------------------------------------------------------
    # coverage target
    #-----------------------------------------------------------------------------
    set(COVERDIR ${CMAKE_BINARY_DIR}/measures/coverage CACHE STRING "Path to the coverage results" FORCE)
    set(COVERAGE_TOP_DIR ${CMAKE_BINARY_DIR})
    set(TEMP_DIR ${CMAKE_BINARY_DIR}/temp)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

    # Run the binary for coverage of the exception in AccessLogParser in case of redirection instead of pipe
    add_custom_target(coverage-unit
      COMMAND make coverage-clean
      COMMAND make -j2 all
      COMMAND make test ARGS='-V'
      COMMAND make coverage-unitP)

    add_custom_target(coverage-functional
      COMMAND make coverage-functional-clean
      COMMAND make -j2 all
      COMMAND make check-functional
      COMMAND make coverage-functionalP
      )

    add_custom_target(coverage-functional-compare
      COMMAND make coverage-functional-compare-clean
      COMMAND make -j2 all
      COMMAND make check-functional-compare
      )

    add_custom_target(coverage-campaign
      COMMAND make coverage
      COMMAND make coverage-functional
      )

    #-----------------------------------------------------------------------------
    # coverage-unit target
    #-----------------------------------------------------------------------------
    configure_file(${INPUT_CMAKE_DIR}/coverage-test.cmake.in ${OUTPUT_CMAKE_DIR}/coverage-unit.cmake IMMEDIATE @ONLY)
    add_custom_target(coverage-unitP
        COMMAND "${CMAKE_COMMAND}" -P ${OUTPUT_CMAKE_DIR}/coverage-unit.cmake)
    # Clean target of unit coverage
    configure_file(${INPUT_CMAKE_DIR}/coverage-clean.cmake.in ${OUTPUT_CMAKE_DIR}/coverage-clean.cmake IMMEDIATE @ONLY)
    add_custom_target(coverage-clean
        COMMAND "${CMAKE_COMMAND}" -P ${OUTPUT_CMAKE_DIR}/coverage-clean.cmake)


    #-----------------------------------------------------------------------------
    # coverage-ft target
    #-----------------------------------------------------------------------------
    set(COVERDIR ${CMAKE_BINARY_DIR}/measures/coverage_ft CACHE STRING "Path to the ft coverage results" FORCE)
    configure_file(${INPUT_CMAKE_DIR}/coverage-test.cmake.in ${OUTPUT_CMAKE_DIR}/coverage-functional.cmake IMMEDIATE @ONLY)
    add_custom_target(coverage-functionalP
        COMMAND "${CMAKE_COMMAND}" -P ${OUTPUT_CMAKE_DIR}/coverage-functional.cmake)
    # Clean target on functional
    configure_file(${INPUT_CMAKE_DIR}/coverage-clean.cmake.in ${OUTPUT_CMAKE_DIR}/coverage-functional-clean.cmake IMMEDIATE @ONLY)
    add_custom_target(coverage-functional-clean
        COMMAND "${CMAKE_COMMAND}" -P ${OUTPUT_CMAKE_DIR}/coverage-functional-clean.cmake)

    # Clean target on functional compare
    configure_file(${INPUT_CMAKE_DIR}/coverage-clean.cmake.in ${OUTPUT_CMAKE_DIR}/coverage-functional-compare-clean.cmake IMMEDIATE @ONLY)
    add_custom_target(coverage-functional-compare-clean
        COMMAND "${CMAKE_COMMAND}" -P ${OUTPUT_CMAKE_DIR}/coverage-functional-compare-clean.cmake)


endif()

if(DEBUG)
  add_definitions(-DDEBUG)
endif()

# add the binary tree to the search path for include files
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")

add_subdirectory(src)
add_subdirectory(src/libws_diff)

# Unit tests
if(BUILD_UNIT_TESTS)
  ENABLE_TESTING()
  add_subdirectory(testing/unit)
  add_subdirectory(testing/libws_diff)
endif()

if(BUILD_COVERAGE)
  add_subdirectory(testing/functionnal)
endif()

add_subdirectory(docs)
add_subdirectory(conf)

##mod_dup tools building
if(BUILD_TOOLS)
	add_subdirectory(tools/libws_diff)
endif()

file(GLOB mod_keyring_metrics src/*.cc src/*.hh)

add_custom_target(metrics
COMMAND mkdir -p ${CMAKE_BINARY_DIR}/measures/complexity
COMMAND pmccabe -vc ${mod_keyring_metrics} 2>/dev/null > ${CMAKE_BINARY_DIR}/measures/complexity/metrics.details.csv;
COMMAND pmccabe -vTF ${mod_keyring_metrics} 2>/dev/null > ${CMAKE_BINARY_DIR}/measures/complexity/metrics.glob.csv;
COMMENT "Calculating McCabe Cyclomatic Complexity\nwritten to ${CMAKE_SOURCE_DIR}/measures/complexity/"
)
