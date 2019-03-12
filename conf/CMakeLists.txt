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

install(FILES compare_test.load compare_test.conf compare.load DESTINATION /etc/apache2/mods-available COMPONENT mod_compare)

install(FILES dup_test.load dup_test.conf dup.load DESTINATION /etc/apache2/mods-available COMPONENT mod_dup)
install(FILES dup_test_vhost.conf DESTINATION /etc/apache2/sites-available COMPONENT mod_dup)

install(FILES migrate_test.load migrate_test.conf migrate.load DESTINATION /etc/apache2/mods-available COMPONENT mod_migrate)
install(FILES migrate_test_vhost.conf DESTINATION /etc/apache2/sites-available COMPONENT mod_migrate)

add_custom_target(install_dup
  COMMAND a2enmod dup
)
add_custom_target(install_dup_test
  COMMAND a2enmod dup_test cgid
  COMMAND a2ensite dup_test_vhost
  DEPENDS install_dup install_mock
)

add_custom_target(install_migrate
  COMMAND a2enmod migrate rewrite proxy proxy_http
)
add_custom_target(install_migrate_test
  COMMAND a2enmod migrate migrate_test rewrite
  COMMAND a2ensite migrate_test_vhost
  DEPENDS install_migrate
)

add_custom_target(install_compare
  COMMAND a2enmod compare
  COMMAND ldconfig
)
add_custom_target(install_compare_test
  COMMAND a2enmod compare_test
  DEPENDS install_compare
)

add_custom_target(install_all
  DEPENDS install_dup install_dup_test install_compare install_compare_test
  DEPENDS install_migrate install_migrate_test install_mock
  COMMAND service apache2 restart
)
