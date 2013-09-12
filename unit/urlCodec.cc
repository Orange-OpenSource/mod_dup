#include "urlCodec.hh"

// urlencode - RFC 1738
// urldecode - RFC 1738

#include <cctype>
#include <stdint.h>


/// @brief performs a url encoding of pIn into pOut
/// @param pOut the output encoded stream
/// @param pIn the input stream
/// @return always true
///
bool urlEncode( std::ostream& pOut, std::istream& pIn )
{
    const char *h = "0123456789abcdef";

    while( true ){
        char c;
        pIn.get(c);

        if( pIn.eof() ){
            break;
        }

        if( isalnum(c) || (c == '-') || (c == '_') || (c == '.') ){
            pOut << c;
        }
        else if( c == ' ' ){
            pOut << '+';
        }
        else {
            pOut << '%' << h[(c >> 4) & 0x0f] << h[c & 0x0f];
        }
    }

    return true;
}


/// @brief performs a url decoding of pIn into pOut
/// @param pOut the output decoded stream
/// @param pIn the input stream
/// @return true upon success, false otherwise
///
bool urlDecode( std::ostream& pOut, std::istream& pIn )
{
    while( true ){
        char c;
        pIn.get(c);

        if( pIn.eof() ){
            break;
        }

        if( c == '%' ){
            char c1, c2;
            pIn >> c1 >> c2;

            if( pIn.eof() ){
                return false;
            }
            c1 = tolower(c1);
            c2 = tolower(c2);

            if( ! isxdigit(c1) || ! isxdigit(c2) ){
                return false;
            }

            c1 = (c1 <= '9') ? (c1 - '0') : (c1 - 'a' + 10);
            c2 = (c2 <= '9') ? (c2 - '0') : (c2 - 'a' + 10);
            pOut << char(16 * c1 + c2);
        }
        else if( c == '+' ){
            pOut << ' ';
        }
        else {
            pOut << c;
        }
    }

    return true;
}


/// @brief performs a html encoding of pIn into pOut
/// @param pOut the output encoded stream
/// @param pIn the input stream
/// @return always true
///
bool htmlEncode( std::ostream& pOut, std::istream& pIn )
{
    const char *h = "0123456789abcdef";

    while( true ){
        char c;
        pIn.get(c);

        if( pIn.eof() ){
            break;
        }

        if( isalnum(c) || (c == '-') || (c == '_') || (c == '.') ){
            pOut << c;
        }
        else {
            pOut << '%' << h[(c >> 4) & 0x0f] << h[c & 0x0f];
        }
    }

    return true;
}


/// @brief performs a html decoding of pIn into pOut
/// @param pOut the output decoded stream
/// @param pIn the input stream
/// @return true upon success, false otherwise
///
bool htmlDecode( std::ostream& pOut, std::istream& pIn )
{
    return urlDecode( pOut, pIn );
}


/// @brief Escape characters that will interfere with xml.
/// @param pOut the output enccoded stream
/// @param pIn the input stream to escape
/// @param pEscQuote wether escape quotes or not
/// @param pAscii7 whether encoding non ascii7 characters (default false)
///
void xmlEscape( std::ostream& pOut, std::istream& pIn, bool pEscQuot, bool pAscii7 )
{
    while( pIn.good() ){

        char c;
        pIn.get(c);

        if( pIn.eof() ){
            break;
        }

        switch( c ){

            // always escaped characters
        case '&': pOut << "&amp;"; break;
        case '<': pOut << "&lt;"; break;
        case '>': pOut << "&gt;"; break;

            // quotes to be escaped only within attributes
        case '"': 
            if( pEscQuot ){
                pOut << "&quot;";
            }
            else {
                pOut << c;
            }
            break;

        case '\'': 
            if( pEscQuot ){
                pOut << "&apos;";
            }
            else {
                pOut << c;
            }
            break;

        default:
            if( pAscii7 && (c<32 || c>127) ){
                pOut << "&#" << uint16_t(c) << ";";
            }
            else {
                pOut << c;
            }
        }
    }
}
