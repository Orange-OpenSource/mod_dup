/*
* mod_dup - duplicates apache requests
* 
* Copyright (C) 2013 Orange
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <boost/scoped_ptr.hpp>

#include "UrlCodec.hh"
#include "testUrlCodec.hh"

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( TestUrlCodec );

using namespace DupModule;


void TestUrlCodec::testUrlCodec()
{
    apr_pool_t *lPool;
    apr_pool_create(&lPool, 0);

	boost::scoped_ptr<const IUrlCodec> urlCodec(getUrlCodec());
	CPPUNIT_ASSERT(urlCodec.get() != NULL);
    CPPUNIT_ASSERT_EQUAL(std::string(" "), urlCodec->decode("%20"));
    CPPUNIT_ASSERT_EQUAL(std::string("%20"), urlCodec->encode(lPool, " "));
}

void TestUrlCodec::testApacheCodec()
{
    apr_pool_t *lPool;
    apr_pool_create(&lPool, 0);

	boost::scoped_ptr<const IUrlCodec> urlCodec(getUrlCodec("apache"));
	CPPUNIT_ASSERT(urlCodec.get() != NULL);
    CPPUNIT_ASSERT_EQUAL(std::string(" "), urlCodec->decode("%20"));
    CPPUNIT_ASSERT_EQUAL(std::string("%20"), urlCodec->encode(lPool, " "));

	// Non-encoded characters:
	CPPUNIT_ASSERT_EQUAL(std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"),
			urlCodec->decode("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"));
	CPPUNIT_ASSERT_EQUAL(std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"),
			urlCodec->encode(lPool, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"));

	CPPUNIT_ASSERT_EQUAL(std::string("!#$&'()*+,/:;=?@[]"),urlCodec->decode("%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"));

	// Case does not matter
	CPPUNIT_ASSERT_EQUAL(std::string("!#$&'()*+,/:;=?@[]"),urlCodec->decode("%21%23%24%26%27%28%29%2a%2b%2c%2f%3a%3b%3d%3f%40%5b%5d"));

	// We do not encode: !$&'()*+,:=@
	CPPUNIT_ASSERT_EQUAL(std::string("!%23$&'()*+,%2f:%3b=%3f@%5b%5d"), urlCodec->encode(lPool, "!#$&'()*+,/:;=?@[]"));

	CPPUNIT_ASSERT_EQUAL(std::string(" \"%-.<>\\^_`{|}~"),urlCodec->decode("%20%22%25%2D%2E%3C%3E%5C%5E%5F%60%7B%7C%7D%7E"));

	// Case does not matter
	CPPUNIT_ASSERT_EQUAL(std::string(" \"%-.<>\\^_`{|}~"),urlCodec->decode("%20%22%25%2d%2e%3c%3e%5c%5e%5f%60%7b%7c%7d%7e"));

	// We do not encode: -._~
	CPPUNIT_ASSERT_EQUAL(std::string("%20%22%25-.%3c%3e%5c%5e_%60%7b%7c%7d~"), urlCodec->encode(lPool, " \"%-.<>\\^_`{|}~"));

	// Make sure it's symetric
	CPPUNIT_ASSERT_EQUAL(std::string("!#$&'()*+,/:;=?@[] \"%-.<>\\^_`{|}~"),
			urlCodec->decode(urlCodec->encode(lPool, "!#$&'()*+,/:;=?@[] \"%-.<>\\^_`{|}~")));
}

void TestUrlCodec::testDefaultCodec()
{
    apr_pool_t *lPool;
    apr_pool_create(&lPool, 0);

	boost::scoped_ptr<const IUrlCodec> urlCodec(getUrlCodec("default"));
	CPPUNIT_ASSERT(urlCodec.get() != NULL);
    CPPUNIT_ASSERT_EQUAL(std::string(" "), urlCodec->decode("%20"));
    CPPUNIT_ASSERT_EQUAL(std::string("%20"), urlCodec->encode(lPool, " "));

	// Non-encoded characters:
	CPPUNIT_ASSERT_EQUAL(std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"),
			urlCodec->decode("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"));
	CPPUNIT_ASSERT_EQUAL(std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"),
			urlCodec->encode(lPool, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"));

	CPPUNIT_ASSERT_EQUAL(std::string("!#$&'()*+,/:;=?@[]"),urlCodec->decode("%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"));

	// Case does not matter
	CPPUNIT_ASSERT_EQUAL(std::string("!#$&'()*+,/:;=?@[]"),urlCodec->decode("%21%23%24%26%27%28%29%2a%2b%2c%2f%3a%3b%3d%3f%40%5b%5d"));

	// We do not encode: !$&'()*,:=@
	CPPUNIT_ASSERT_EQUAL(std::string("!%23$&'()*%2b,%2f:%3b=%3f@%5b%5d"), urlCodec->encode(lPool, "!#$&'()*+,/:;=?@[]"));

	CPPUNIT_ASSERT_EQUAL(std::string(" \"%-.<>\\^_`{|}~"),urlCodec->decode("%20%22%25%2D%2E%3C%3E%5C%5E%5F%60%7B%7C%7D%7E"));

	// Case does not matter
	CPPUNIT_ASSERT_EQUAL(std::string(" \"%-.<>\\^_`{|}~"),urlCodec->decode("%20%22%25%2d%2e%3c%3e%5c%5e%5f%60%7b%7c%7d%7e"));

	// We do not encode: -._~
	CPPUNIT_ASSERT_EQUAL(std::string("%20%22%25-.%3c%3e%5c%5e_%60%7b%7c%7d~"), urlCodec->encode(lPool, " \"%-.<>\\^_`{|}~"));

	// Make sure it's symetric
	CPPUNIT_ASSERT_EQUAL(std::string("!#$&'()*+,/:;=?@[] \"%-.<>\\^_`{|}~"),
			urlCodec->decode(urlCodec->encode(lPool, "!#$&'()*+,/:;=?@[] \"%-.<>\\^_`{|}~")));
}
