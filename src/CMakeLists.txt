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

include(../cmake/Include.cmake)
include_directories(${PROJECT_SOURCE_DIR}/src)

file(GLOB mod_dup_SOURCE_FILES
  RequestCommon.cc
  filters_dup.cc
  mod_dup.cc
  Log.cc
  RequestProcessor.cc
  RequestInfo.cc
  Utils.cc
  ThreadPool.cc
  MultiThreadQueue.cc
  UrlCodec.cc)

file(GLOB mod_compare_SOURCE_FILES
  CassandraDiff.cc
  filters_compare.cc
  mod_compare.cc
  response_diff.cc
  deserialize.cc
  Log.cc
  Utils.cc
  ThreadPool.cc
  MultiThreadQueue.cc
  RequestInfo.cc)

file(GLOB mod_migrate_SOURCE_FILES
  filters_migrate.cc
  mod_migrate.cc
  Log.cc
  RequestCommon.cc
  RequestInfo.cc
  Utils.cc
  ThreadPool.cc
  MultiThreadQueue.cc
  UrlCodec.cc)


# find_package(Boost REQUIRED COMPONENTS regex serialization)
#include_directories(${Boost_INCLUDE_DIRS})

SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libboost-regex1.40-dev | libboost-regex1.46-dev, libboost-serialization1.40-dev | libboost-serialization1.46-dev")

# Compile as library
add_library(mod_dup MODULE ${mod_dup_SOURCE_FILES})
set_target_properties(mod_dup PROPERTIES PREFIX "")
target_link_libraries(mod_dup ${APR_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES} boost_regex boost_thread)

add_library(mod_compare MODULE ${mod_compare_SOURCE_FILES})
set_target_properties(mod_compare PROPERTIES PREFIX "")
target_link_libraries(mod_compare ${APR_LIBRARIES} ${Boost_LIBRARIES} boost_serialization libws_diff rt boost_date_time)

add_library(mod_migrate MODULE ${mod_migrate_SOURCE_FILES})
set_target_properties(mod_migrate PROPERTIES PREFIX "")
target_link_libraries(mod_migrate ${APR_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES} boost_regex boost_thread rt)

install(TARGETS mod_dup LIBRARY DESTINATION /usr/lib/apache2/modules COMPONENT mod_dup)
install(TARGETS mod_compare LIBRARY DESTINATION /usr/lib/apache2/modules COMPONENT mod_compare)
install(TARGETS mod_migrate LIBRARY DESTINATION /usr/lib/apache2/modules COMPONENT mod_migrate)
