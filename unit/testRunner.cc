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

#include <string>
using std::string;

// cppunit
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/XmlOutputter.h>

#include "unixd.h"
#include <pwd.h>

extern unixd_config_rec unixd_config;

int main( int argc, char* argv[])
{
    bool outToXml = false;
    if ( (argc > 1) && (string(argv[1]) == "-x") ) {
        outToXml = true;
    }

    struct passwd *lPwd = getpwuid(getuid());
    unixd_config.user_id = lPwd->pw_uid;
    unixd_config.user_name = lPwd->pw_name;
    unixd_config.group_id = lPwd->pw_gid;

    std::string outFile(argv[0]);
    outFile += ".xml";
    std::ofstream xmlFileOut;

    CppUnit::TextUi::TestRunner runner;
    if ( outToXml ) {
        xmlFileOut.open(outFile.c_str());
        if (xmlFileOut.fail()) {
            return 1;
        }
        runner.setOutputter(new CppUnit::XmlOutputter(&runner.result(), xmlFileOut));
    }
    runner.addTest( CppUnit::TestFactoryRegistry::getRegistry().makeTest() );
    int lRet =  runner.run();

    if ( outToXml ) {
        xmlFileOut.close();
    }

    if ( lRet ) {
        return 0;
    }
    else {
        return 1;
    }
}
