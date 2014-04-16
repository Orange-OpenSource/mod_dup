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

file(GLOB libws_diff_main_SOURCE_FILES
	lib_compare.cc
	${PROJECT_SOURCE_DIR}/build/src/libws_diff
  )

include_directories(${PROJECT_SOURCE_DIR}/extern/dtl-cpp/dtl)
include_directories(${PROJECT_SOURCE_DIR}/src/libws_diff)
include_directories(${PROJECT_SOURCE_DIR}/src)

# Compile as exec
add_executable(libws_diff_main ${libws_diff_main_SOURCE_FILES})
target_link_libraries(libws_diff_main libws_diff ${cppunit_LIBRARY})

install(TARGETS libws_diff_main DESTINATION bin COMPONENT libws_diff)