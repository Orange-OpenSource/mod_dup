# Try to find cppunit
# Once done, this will define :
# cppunit_FOUND - system has cppunit
# cppunit_INCLUDE_DIRS - the cppunit include directories
# cppunit_LIBRARIES - link these to use cppunit

find_path(cppunit_INCLUDE_DIR cppunit/TestCase.h /usr/local/include /usr/include)

find_library(cppunit_LIBRARY cppunit ${cppunit_INCLUDE_DIR}/../lib /usr/local/lib /usr/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(cppunit DEFAULT_MSG
                                  cppunit_LIBRARY cppunit_INCLUDE_DIR)

mark_as_advanced(cppunit_INCLUDE_DIR cppunit_LIBRARY)

