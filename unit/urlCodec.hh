#pragma once

#include <iostream>

// encodes input stream (same as php urlencode)
// pOut the output stream
// pIn the input stream
// return always true
//
extern bool urlEncode( std::ostream& pOut, std::istream& pIn );

// decodes input stream (same as php urldecode)
// pOut the output stream
// pIn the input stream
// return true upon success false otherwise
//
extern bool urlDecode( std::ostream& pOut, std::istream& pIn );

// encodes input stream (same as php urlencode)
// pOut the output stream
// pIn the input stream
// return always true
//
extern bool htmlEncode( std::ostream& pOut, std::istream& pIn );

// decodes input stream (same as apache htmldecode)
// pOut the output stream
// pIn the input stream
// return true upon success false otherwise
//
extern bool htmlDecode( std::ostream& pOut, std::istream& pIn );


// escapes input stream for XML 
// pOut the output stream
// pIn the input stream
//
void xmlEscape( std::ostream& pOut, std::istream& pIn, bool pEscQuot = false, bool pAscii7 = false );
