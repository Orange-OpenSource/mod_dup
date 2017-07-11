Description
===========
This directory contains functionnal tests for mod_dup, mod_compare and mod_migrate
The install in apache is automatic only on Ubuntu/Debian platforms using the package 

mod_dup
=======
in mod_dup root, run dpkg-buildpackage
in your workspace above the mod_dup root you will find several deb files
Install mod_dup and validation package: sudo dpkg -i libapache2-mod-dup-migrate-validation_* libapache2-mod-dup-*
Restart apache: sudo service apache2 restart
Run tests: ./test_dup.py

mod_compare
===========
//TODO ADD DOC

mod_migrate
=======
//TODO ADD DOC