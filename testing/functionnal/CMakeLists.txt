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

## Stops the system apache that uses the code coverage configuration
add_custom_target(apache_functional_tests_stop
  COMMAND /usr/sbin/apache2ctl -f ${CMAKE_BINARY_DIR}/apache2/httpd_func_tests.conf -k stop
  COMMAND sleep 1
  )

## Collecting the functionnal directories
file(GLOB test_dirs "${CMAKE_SOURCE_DIR}/testing/functionnal/data/dup/*" )

## Runs the functional tests
## Iterate over the directories in the functional repo to launch the corresponding tests
add_custom_target(check_functional)

foreach(td ${test_dirs})
  add_custom_command(TARGET check_functional PRE_BUILD
    COMMAND make apache_functional_tests_stop

    ## Prepare apache
    COMMAND cd ${CMAKE_SOURCE_DIR}/testing/functionnal/ && ./env.sh ../../build/src ${td}

    ## Launch apache
    COMMAND /usr/sbin/apache2 -f ${CMAKE_BINARY_DIR}/apache2/httpd_func_tests.conf

    ## Launching the tests script
    COMMAND ${CMAKE_SOURCE_DIR}/testing/functionnal/test_dup.py --path=${td}/requests/

    ## Stopping apache
    COMMAND make apache_functional_tests_stop
  )
endforeach()
