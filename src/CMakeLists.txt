cmake_minimum_required(VERSION 2.8)

file(GLOB mod_dup_SOURCE_FILES mod_dup.cc Log.cc RequestProcessor.cc)

find_package( Boost REQUIRED COMPONENTS thread regex )
include_directories( ${Boost_INCLUDE_DIRS} )
include_directories(/usr/include/apr-1.0)
include_directories(/usr/include/apache2)

# Compile as library
add_library(mod_dup MODULE ${mod_dup_SOURCE_FILES})
set_target_properties(mod_dup PROPERTIES PREFIX "")
target_link_libraries(mod_dup apr-1 ${Boost_LIBRARIES} curl)
