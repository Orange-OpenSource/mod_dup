cmake_minimum_required(VERSION 2.8)

add_definitions(-DUNIT_TESTING)

include_directories("../src/")
include_directories(".")
include_directories( ${Boost_INCLUDE_DIRS} )
include_directories(/usr/include/apr-1.0)
include_directories(/usr/include/apache2)

# UNIT TESTS
file(GLOB lib_SOURCE_FILES ../src/mod_dup.cc ../src/Log.cc ../src/RequestProcessor.cc)
find_package( Boost REQUIRED COMPONENTS thread regex )

add_library(mod_dup_lib SHARED ApacheStubs.cc ApacheCopyPaste.cc urlCodec.cc ${lib_SOURCE_FILES})
set_target_properties(mod_dup_lib PROPERTIES PREFIX "")
target_link_libraries(mod_dup_lib apr-1 ${Boost_LIBRARIES} curl)

file(GLOB mod_dup_test_SOURCE_FILES	testThreadPool.cc
								testMultiThreadQueue.cc
								testRequestProcessor.cc
								testLog.cc
								testModDup.cc
								testRunner.cc)
add_executable(mod_dup_test ${mod_dup_test_SOURCE_FILES})
target_link_libraries(mod_dup_test mod_dup_lib cppunit boost_thread apr-1)
add_test(mod_dup_UnitTestInit rm -f mod_dup_unittest.file)
add_test(mod_dup_UnitTestsFirstRun mod_dup_test -x)
