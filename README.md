Description
===========

Apache module (mod_dup) duplicating requests before they are handled by their usual module in Apache.
Useful for testing new servers/modules with a real production load but no user impact.
Request parameters (GET & POST) can be rewritten before duplication.

Basic Request Duplication
=========================
![](https://raw2.github.com/Orange-OpenSource/mod_dup/master/docs/mod_dup_overview.png)

Documentation
=============

To generate the technical documentation of the code using doxygen, see documentation in the Build section.

For usage and configuration documentation, read the module Wiki at this page https://github.com/Orange-OpenSource/mod_dup/wiki


Build
=====

First create and go into a build directory:

	mkdir build
	cd build

then run cmake & make:

	cmake ..
	make

or with unit tests:

	cmake -DBUILD_UNIT_TESTS=ON ..
	make test
	CTEST_OUTPUT_ON_FAILURE=1 make test

or with coverage:

	cmake -DBUILD_COVERAGE=ON ..
	make coverage
	CTEST_OUTPUT_ON_FAILURE=1 make test

	<browser> measures/coverage/html/index.html

or to generate documentation:

	cmake ..
	make doc

	<browser> docs/doxygen/html/index.html

Dependencies
============

Build time
----------

	cmake
	libcurl4-openssl-dev
	libboost-thread-dev
	libboost-regex1.40-dev
	libboost-dev
	libapr1-dev
	libaprutil1-dev
	apache2-threaded-dev OR apache2-prefork-dev
	gcc (>= 4.4.3)

Run time
--------

	apache2.2-common
	libboost-thread1.40.0
	libboost-regex1.40.0
	libcurl3
