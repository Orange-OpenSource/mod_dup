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
