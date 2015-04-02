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

cmake_minimum_required(VERSION 2.8)

include(${PROJECT_SOURCE_DIR}/cmake/Include.cmake)

find_package(cppunit REQUIRED)

add_definitions(-DUNIT_TESTING)

include_directories("${PROJECT_SOURCE_DIR}/src/libws_diff")
include_directories("${PROJECT_SOURCE_DIR}/testing/libws_diff")
include_directories("${PROJECT_SOURCE_DIR}/src/libws_diff/DiffPrinter")
include_directories("${PROJECT_SOURCE_DIR}/testing/libws_diff/DiffPrinter")

# UNIT TESTS
file(GLOB lib_ws_diff_test_SOURCE_FILES
  testWsStringDiff.cc
  testWsMapDiff.cc
  testJsonDiffPrinter.cc
  testRunner.cc
  )
  
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/DiffPrinter)

add_executable(libws_diff_test ${lib_ws_diff_test_SOURCE_FILES})
target_link_libraries(libws_diff_test libws_diff pns_jsoncpp ${cppunit_LIBRARY} ${Boost_LIBRARIES})
add_test(testLibCompare libws_diff_test)

#add_test(libws_diff_UnitTestInit rm -f libws_diff_unittest.file)
#add_test(libws_diff_UnitTestsFirstRun libws_diff_test -x)