# lib_ws_diff - duplicates apache requests
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

file(GLOB libws_diff_SOURCE_FILES
	stringCompare.cc
	mapCompare.cc
  )
  
include_directories(${PROJECT_SOURCE_DIR}/extern/dtl-cpp/dtl)

# Compile as library
add_library(libws_diff SHARED ${libws_diff_SOURCE_FILES})
set_target_properties(libws_diff PROPERTIES PREFIX "")
target_link_libraries(libws_diff ${Boost_LIBRARIES})

file(GLOB libws_diff_HEADER_FILES
	stringCompare.hh
	mapCompare.hh
  )

install(TARGETS libws_diff LIBRARY DESTINATION lib COMPONENT libws_diff)
install(FILES ${libws_diff_HEADER_FILES} DESTINATION include/libws_diff COMPONENT libws_diff)
