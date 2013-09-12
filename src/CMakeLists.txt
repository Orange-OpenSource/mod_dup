cmake_minimum_required(VERSION 2.8)

include(../cmake/Include.cmake)

file(GLOB mod_dup_SOURCE_FILES mod_dup.cc Log.cc RequestProcessor.cc)

# Compile as library
add_library(mod_dup MODULE ${mod_dup_SOURCE_FILES})
set_target_properties(mod_dup PROPERTIES PREFIX "")
target_link_libraries(mod_dup ${APR_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES})
