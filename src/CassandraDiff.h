#include "boost/tuple/tuple.hpp"
#include "boost/thread/mutex.hpp"


namespace CassandraDiff {

/// Field Information enough to know what/why there is a difference
/// Between DB value and Request value
class FieldInfo
{
public:
    FieldInfo(const std::string & name, const std::string & key, const std::string & dbvalue, const std::string & reqvalue) :
        mName(name),
        mMultivalueKey(key),
        mDBValue(dbvalue),
        mReqValue(reqvalue) {};
        
public:
    /// Field name in the db
    std::string mName;
    /// Multivalue/Collection index/key
    std::string mMultivalueKey;
    /// Value retrieved in Database
    std::string mDBValue;
    /// Value about to be set from Request
    std::string mReqValue;
};

/// MultiMap of UNIQUE_ID to RequestInfo
/// There can be more than one request per UNIQUE_ID, with N fields per requests
class Differences: public std::multimap<std::string, FieldInfo>
{
public:
    boost::mutex & getMutex() { return mMutex;};
private:
    boost::mutex mMutex;
};

}