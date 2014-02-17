#include "CassandraDiff.h"


namespace CassandraDiff {

std::ostream& operator<<(std::ostream & strm, const FieldInfo& f){
    strm << "Field name in the db : '" << f.mName << "'\n";
    strm << "Multivalue/Collection index/key : '" << f.mMultivalueKey << "'\n";
    strm << "Value retrieved in Database : '" << f.mDBValue << "'\n";
    strm << "Value about to be set from Request : '" << f.mReqValue << "'\n";
	return strm;
}

}
