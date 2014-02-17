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

file(GLOB mod_dup_SOURCE_FILES
  filters.cc
  mod_dup.cc
  Log.cc
  RequestProcessor.cc
  RequestInfo.cc
  Utils.cc
  UrlCodec.cc)

file(GLOB mod_compare_SOURCE_FILES
  CassandraDiff.cc
  filters_compare.cc
  mod_compare.cc
  Log.cc
  Utils.cc
  RequestInfo.cc)

# Compile as library
add_library(mod_dup MODULE ${mod_dup_SOURCE_FILES})
set_target_properties(mod_dup PROPERTIES PREFIX "")
target_link_libraries(mod_dup ${APR_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES})

add_library(mod_compare MODULE ${mod_compare_SOURCE_FILES})
set_target_properties(mod_compare PROPERTIES PREFIX "")
target_link_libraries(mod_compare ${APR_LIBRARIES} ${Boost_LIBRARIES} ws_diff)
