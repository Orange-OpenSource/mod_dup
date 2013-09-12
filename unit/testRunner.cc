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
