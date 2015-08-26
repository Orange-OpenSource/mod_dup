Description
===========
This repository contains three Apache C++ modules, mod_dup, mod_migrate and mod_compare which duplicate, redirect and compare HTTP requests  respectively.

mod_dup
=======
mod_dup duplicates Apache requests (POST, GET or both).
Only requests which match specified filters are duplicated.
Before duplication, all defined substitutions are applied to the incoming request.
To minimize resource usage, mod_dup adapts to the amount of incoming by adjusting its number of threads.
If maximum thresholds are reached, requests are dropped.
In other words, mod_dup is built to guarantee a low system impact by sacrificing the reliability of duplications.
However, by using a high number of maximum threads, request dropping can be avoided and system impact raised.
mod_dup periodically emits log messages containing metrics such as the number of dropped requests.

mod_compare
===========
mod_compare allows to compare the response header and body of HTTP Requests between two web services.
mod_compare receives an http request which contains the response header and body of a Web Service that will be compared to the response of the web service installed in the same server of mod_compare.
In order to work fine, the input request must contain the following header:
 * Duplication Type: Response
and must respect the "dup format":
 * URL --> the URL of orifinal request 
 * BODY --> XXXXXXXX{request_body}XXXXXXXX{response_header}XXXXXXXX{response_body} 
The first 8 characters indicate the size of the request body. Then the request body, 8 characters for the size of the response header, the response header, 8 characters for the size of the response body and the response body.
Two operating modes are possible: 
 * Response Comparison
 * No Comparison

WARNING: The Web Services must not be CGI based scripts.

mod_migrate
=======
mod_migrate allows redirecting requests based on the body content, on top of mod_rewrite which can only match URLs and Headers. Hence all HTTP Methods may be redirected or proxied (GET, POST, PUT, PATCH...).
Only requests which match specified filters are migrated.
This mod uses mod_rewrite and mod_proxy to redirect the requests.

Basic Request Migration
=========================
![](https://raw.githubusercontent.com/Orange-OpenSource/mod_dup/multidest/docs/mod_migrate.png)

Basic Request Duplication with Migration
=========================
![](https://raw.githubusercontent.com/Orange-OpenSource/mod_dup/master/docs/mod_dup_overview.png)

Duplication with Response
=========================
![](https://raw.githubusercontent.com/Orange-OpenSource/mod_dup/master/docs/dup_comp.png)

Documentation
=============

To generate the technical documentation of the code using doxygen, see documentation in the Build section.

For usage and configuration documentation, read the module's Wiki at this page https://github.com/Orange-OpenSource/mod_dup/wiki


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
	make coverage-unit (or make coverage-functional, make coverage-functional-compare, make coverage-compaign)
	CTEST_OUTPUT_ON_FAILURE=1 make test

	<browser> measures/coverage/html/index.html

or to generate documentation:

	cmake ..
	make doc

	<browser> docs/doxygen/html/index.html

Packaging
=========
A debian folder compatible with Ubuntu Precise and Trusty is present

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
