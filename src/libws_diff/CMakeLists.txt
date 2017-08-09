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
	${CMAKE_CURRENT_SOURCE_DIR}/stringCompare.cc
	${CMAKE_CURRENT_SOURCE_DIR}/mapCompare.cc
  )
  
file(GLOB libws_diff_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/stringCompare.hh
	${CMAKE_CURRENT_SOURCE_DIR}/mapCompare.hh
  )  

#Include file from Diff Printer and its dependancy on extern tool utf8
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/DiffPrinter)
include_directories(${PROJECT_SOURCE_DIR}/extern/utf8/source)
include_directories(${PROJECT_SOURCE_DIR}/extern/utf8/source/utf8)

include_directories(${PROJECT_SOURCE_DIR}/testing/libws_diff/DiffPrinter)
include_directories(${PROJECT_SOURCE_DIR}/extern/dtl-cpp/dtl)



# Compile as library
add_library(libws_diff SHARED ${libws_diff_SOURCE_FILES})
set_target_properties(libws_diff PROPERTIES PREFIX "")
if ( ${PNS_JSONCPP_FOUND} )
target_link_libraries(libws_diff ${PNS_JSONCPP_LIBRARIES} ${Boost_LIBRARIES})
else()
target_link_libraries(libws_diff ${JSONCPP_LIBRARIES} ${Boost_LIBRARIES})
endif()
install(TARGETS libws_diff LIBRARY DESTINATION lib COMPONENT libws_diff)
install(FILES ${libws_diff_HEADER_FILES} DESTINATION include/libws_diff COMPONENT libws_diff)
