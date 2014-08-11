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

## Collecting the functionnal directories
file(GLOB test_dirs "${CMAKE_SOURCE_DIR}/testing/functionnal/data/dup/*" )

## Runs the functional tests
## Iterate over the directories in the functional repo to launch the corresponding tests
add_custom_target(check-functional)

find_program(APACHE2BIN apache2)

foreach(td ${test_dirs})
  add_custom_command(TARGET check-functional PRE_BUILD

    ## Prepare apache
    COMMAND cd ${CMAKE_SOURCE_DIR}/testing/functionnal/ && ./env.sh ${CMAKE_BINARY_DIR}/src ${td}

    ## Launch apache
    COMMAND ${APACHE2BIN} -f ${CMAKE_BINARY_DIR}/apache2/httpd_func_tests.conf

    ## Launching the tests script
    COMMAND ${CMAKE_SOURCE_DIR}/testing/functionnal/test_dup.py --path=${td}/requests/

    COMMAND ${APACHE2BIN} -f ${CMAKE_BINARY_DIR}/apache2/httpd_func_tests.conf -k stop
    ## Apache sometimes needs some time to stop
    COMMAND sleep 2
  )
endforeach()

add_custom_target(check-functional-describe)

foreach(td ${test_dirs})
  add_custom_command(TARGET check-functional-describe PRE_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/testing/functionnal/test_dup.py --path=${td}/requests/ --desc=true
  )
endforeach()


add_custom_target(check-functional-compare)

add_custom_command(TARGET check-functional-compare PRE_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/testing/functionnal/test_compare.sh ${CMAKE_SOURCE_DIR}/testing/functionnal/data/compare/
  )


add_custom_target(check-functional-migrate
COMMAND make install

# make backup of migrate.conf, if it exists
COMMAND
if [ -e /etc/apache2/mods-available/migrate.conf ]\; then
	mv -fv /etc/apache2/mods-available/migrate.conf /etc/apache2/mods-available/migrate.conf.bak\;
fi

COMMAND cp -v ${CMAKE_SOURCE_DIR}/conf/migrate.conf.example /etc/apache2/mods-available/migrate.conf
COMMAND cp -fv ${CMAKE_SOURCE_DIR}/testing/functionnal/print_content.cgi /usr/lib/cgi-bin
COMMAND cp -fv ${CMAKE_SOURCE_DIR}/testing/functionnal/test_migrate_vhost /etc/apache2/sites-available
COMMAND a2ensite test_migrate_vhost
COMMAND a2enmod migrate
COMMAND service apache2 restart

# backup restore handled in the script
COMMAND ${CMAKE_SOURCE_DIR}/testing/functionnal/test_migrate.sh ${CMAKE_SOURCE_DIR}/testing/functionnal/TestMigrate.jmx

COMMAND a2dissite test_migrate_vhost
COMMAND service apache2 reload
)
