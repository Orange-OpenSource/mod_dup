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

install(FILES
  data/dup/www/compare
  data/dup/www/dup
  data/dup/www/enrich
  data/dup/www/header_and_body
  data/dup/www/header_only
  data/dup/www/largeresponse
  data/dup/www/multi
  data/dup/www/rewritten
  data/dup/www/service_np1
  DESTINATION /var/www/dup_test COMPONENT mod_dup)

install(FILES
  data/dup/www/get_body_size.cgi
  PERMISSIONS WORLD_EXECUTE WORLD_READ GROUP_EXECUTE GROUP_READ OWNER_EXECUTE OWNER_READ
  DESTINATION /var/www/dup_test/cgi COMPONENT mod_dup)


## Runs dup functional tests
add_custom_target(check-functional-dup
   ## Launching the tests script
    COMMAND ${CMAKE_SOURCE_DIR}/testing/functionnal/test_dup.py --path=${CMAKE_SOURCE_DIR}/testing/functionnal/data/dup/requests/
)

add_custom_target(check-functional-dup-describe
    COMMAND ${CMAKE_SOURCE_DIR}/testing/functionnal/test_dup.py --path=${CMAKE_SOURCE_DIR}/testing/functionnal/data/dup/requests/ --desc=true
)

add_custom_target(check-functional-compare
    COMMAND ${CMAKE_SOURCE_DIR}/testing/functionnal/test_compare.sh ${CMAKE_SOURCE_DIR}/testing/functionnal/data/compare/
)

add_custom_target(check-functional-migrate
  COMMAND echo Starting mod_migrate functional test...
  COMMAND echo Setting up the test environment...

  ## Prepare apache
  COMMAND cd ${CMAKE_SOURCE_DIR}/testing/functionnal/ && ./env_migrate.sh ${CMAKE_BINARY_DIR}/src ${CMAKE_SOURCE_DIR}/testing/functionnal/data/migrate/

  COMMAND echo "Starting Apache with the test conf..."
  ## Launch apache
  COMMAND ${APACHE2BIN} -f ${CMAKE_BINARY_DIR}/apache2/httpd_func_tests.conf

  COMMAND echo "Starting JMeter test..."
  # backup restore handled in the script
  COMMAND ${CMAKE_SOURCE_DIR}/testing/functionnal/test_migrate_for_jenkins.sh ${CMAKE_SOURCE_DIR}/testing/functionnal

  COMMAND echo "Stoping Apache..."
  COMMAND ${APACHE2BIN} -f ${CMAKE_BINARY_DIR}/apache2/httpd_func_tests.conf -k stop

  ## Apache sometimes needs some time to stop
  COMMAND sleep 2
)

#COMMAND make install

# make backup of migrate.conf, if it exists
#COMMAND
#if [ -e /etc/apache2/mods-available/migrate.conf ]\; then
#	mv -fv /etc/apache2/mods-available/migrate.conf /etc/apache2/mods-available/migrate.conf.bak\;
#fi

#COMMAND cp -v ${CMAKE_SOURCE_DIR}/conf/migrate.conf.example /etc/apache2/mods-available/migrate.conf
#COMMAND cp -fv ${CMAKE_SOURCE_DIR}/testing/functionnal/print_content.cgi /usr/lib/cgi-bin
#COMMAND cp -fv ${CMAKE_SOURCE_DIR}/testing/functionnal/test_migrate_vhost /etc/apache2/sites-available
#COMMAND a2ensite test_migrate_vhost
#COMMAND a2enmod migrate
#COMMAND service apache2 restart

# backup restore handled in the script
#COMMAND ${CMAKE_SOURCE_DIR}/testing/functionnal/test_migrate.sh ${CMAKE_SOURCE_DIR}/testing/functionnal/TestMigrate.jmx

#COMMAND a2dissite test_migrate_vhost
#COMMAND service apache2 reload
#)
