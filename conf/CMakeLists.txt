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

install(FILES compare_test.load compare_test.conf DESTINATION /etc/apache2/mods-available COMPONENT mod_compare)

install(FILES dup_test.load dup_test.conf DESTINATION /etc/apache2/mods-available COMPONENT mod_dup)
install(FILES dup_test_vhost DESTINATION /etc/apache2/sites-available COMPONENT mod_dup)

install(FILES migrate_test.load migrate_test.conf DESTINATION /etc/apache2/mods-available COMPONENT mod_migrate)

