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


## Configures system apache and launches it
add_custom_target(apache_functional_tests_setup
  ## Prepare apache
  COMMAND cd ../../../testing/functionnal/ && ./env.sh ../../build/src ./data/dup/0/

  COMMAND /usr/sbin/apache2 -f ${CMAKE_BINARY_DIR}/apache2/httpd_func_tests.conf

  )

## Stopping apache before the setup
add_dependencies(apache_functional_tests_setup apache_functional_tests_stop)

## Stops the system apache that uses the code coverage configuration
add_custom_target(apache_functional_tests_stop
  COMMAND /usr/sbin/apache2ctl -f ${CMAKE_BINARY_DIR}/apache2/httpd_func_tests.conf -k stop
  )

