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

include(../../cmake/Include.cmake)

find_package(cppunit REQUIRED)

add_definitions(-DUNIT_TESTING)

include_directories("../../src/")
include_directories(".")

# UNIT TESTS
file(GLOB lib_SOURCE_FILES
  ../../src/mod_dup.cc
  ../../src/Log.cc
  ../../src/RequestProcessor.cc
  ../../src/RequestInfo.cc
  ../../src/UrlCodec.cc
  ../../src/filters.cc
  ../../src/mod_compare.cc
  ../../src/filters_compare.cc
  ../../src/CassandraDiff.cc
  ../../src/Utils.cc
)

add_library(mod_dup_lib SHARED ApacheStubs.cc ApacheCopyPaste.cc urlCodec.cc ${lib_SOURCE_FILES})

set_target_properties(mod_dup_lib PROPERTIES PREFIX "")
target_link_libraries(mod_dup_lib ${APR_LIBRARIES} ${APRUTIL_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES})

file(GLOB mod_dup_test_SOURCE_FILES
  testThreadPool.cc
  testMultiThreadQueue.cc
  testRequestProcessor.cc
  testLog.cc
  testUrlCodec.cc
  testModDup.cc
  testRunner.cc
  testContextEnrichment.cc
  testBodies.cc
  testFilters.cc
  testModCompare.cc
)

add_executable(mod_dup_test ${mod_dup_test_SOURCE_FILES})

target_link_libraries(mod_dup_test mod_dup_lib ${cppunit_LIBRARY} ${Boost_LIBRARIES} ${APR_LIBRARIES} ${APRUTIL_LIBRARIES} ws_diff)

add_test(mod_dup_UnitTestInit rm -f mod_dup_unittest.file)
add_test(mod_dup_UnitTestsFirstRun mod_dup_test -x)

