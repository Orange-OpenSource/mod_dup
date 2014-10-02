
## Web service mock

include(${CMAKE_SOURCE_DIR}/cmake/Include.cmake)

file( GLOB dup_mock_src_files
  dup_mock.cc
  )

add_library(dup_mock MODULE ${dup_mock_src_files})
set_target_properties(dup_mock PROPERTIES PREFIX "")

install(TARGETS dup_mock LIBRARY DESTINATION /usr/lib/apache2/modules COMPONENT debug)
install(FILES dup_mock.load DESTINATION /etc/apache2/mods-available COMPONENT debug)
