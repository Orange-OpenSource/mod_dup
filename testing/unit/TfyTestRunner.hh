#pragma once

#include <ostream>

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/XmlOutputter.h>

#include <iostream>

#include <cstdlib>
#include <cstring>

class TfyTestRunner :
    public CppUnit::TextTestRunner
{

public:
    inline TfyTestRunner(std::ostream& pOut = std::cout);
    inline TfyTestRunner(const char* pName = NULL);

    virtual ~TfyTestRunner()
    {
        delete out;
    }

private:
    std::ostream* out;
};


//==============================================
//==============================================


inline TfyTestRunner::TfyTestRunner(std::ostream& pOut) :
    out(NULL)
{
    const char* lOut = getenv("UT_OUTPUT");

    if( (! lOut) || (strcasecmp(lOut, "xml") != 0) ){
        return;
    }

    setOutputter(new CppUnit::XmlOutputter( &result(), pOut));
}


inline TfyTestRunner::TfyTestRunner(const char* pName) :
    out(NULL)
{
    const char* lOut = getenv("UT_OUTPUT");

    if( (! lOut) || (strcasecmp(lOut, "xml") != 0) ){
        return;
    }

    std::ostream* lOStream = &std::cout;

    if( pName ){
        std::string lFile = std::string(pName) + std::string(".xml");
        out = new std::ofstream(lFile.c_str());
        lOStream = out;
    }

    setOutputter(new CppUnit::XmlOutputter( &result(), *lOStream));
}


