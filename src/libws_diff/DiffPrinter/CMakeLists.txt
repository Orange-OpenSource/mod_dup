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

set(libws_diff_SOURCE_FILES
   ${libws_diff_SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/diffPrinter.cc
   ${CMAKE_CURRENT_SOURCE_DIR}/jsonDiffPrinter.cc
   ${CMAKE_CURRENT_SOURCE_DIR}/utf8JsonDiffPrinter.cc
   ${CMAKE_CURRENT_SOURCE_DIR}/multilineDiffPrinter.cc
   PARENT_SCOPE
)

set(libws_diff_HEADER_FILES
   ${libws_diff_HEADER_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/diffPrinter.hh
   ${CMAKE_CURRENT_SOURCE_DIR}/jsonDiffPrinter.hh
   ${CMAKE_CURRENT_SOURCE_DIR}/utf8JsonDiffPrinter.hh
   ${CMAKE_CURRENT_SOURCE_DIR}/multilineDiffPrinter.hh
   ${PROJECT_SOURCE_DIR}/extern/utf8/source/utf8.h
   PARENT_SCOPE
)