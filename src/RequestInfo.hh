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

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/basic_text_oarchive.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/shared_ptr.hpp>

#include <list>
#include <map>
#include <string>
#include <sstream>


struct apr_bucket_brigade;

namespace MigrateModule {
struct RequestInfo {
    RequestInfo(std::string pId, int64_t startTime) : mId(pId), mConf(nullptr) {}
    /** @brief The query unique ID. */
    std::string mId;
    /** @brief The body part of the query */
    std::string mBody;
    /** @brief The parameters part of the query (without leading ?). */
    std::string mArgs;
    /** @brief string that represents the http header of the incoming request, e.g. "Content-Type: text/html\r\nHeader-key: Header-value*/
    std::string mHeader;
    /** @brief The conf object for the location of this Request */
    void * mConf;
    
};
}

namespace DupModule {

/*
 * Different duplication modes supported by mod_dup
 */
namespace DuplicationType {

enum eDuplicationType {
    NONE                    = 0,    // Do not duplicate, is the default or used in enrichment mode
    HEADER_ONLY             = 1,    // Duplication only the HTTP HEADER of matching requests
    COMPLETE_REQUEST        = 2,    // Duplication HTTP HEADER AND BODY of matching requests
    REQUEST_WITH_ANSWER     = 3,    // Duplication HTTP REQUEST AND ANSWER of matching requests
};

/*
 * Converts the string representation of a DuplicationType into the enum value
 */
eDuplicationType stringToEnum(const char *value) throw (std::exception);

};

/**
 * @brief Contains information about the incoming request.
 */
struct RequestInfo {

    typedef std::map<std::string,std::string> mapStr;

    friend class boost::serialization::access;
    // cf http://www.boost.org/doc/libs/1_55_0/libs/serialization/doc/tutorial.html
    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template<typename Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & mRequest;
        ar & mReqHeader;
        ar & mReqBody;
        ar & mResponseHeader;
        ar & mResponseBody;
        ar & mDupResponseHeader;
        ar & mDupResponseBody;
    }

    /** @brief True if the request processor should stop ater seeing this object. */
    bool mPoison;
    /** @brief The query unique ID. */
    std::string mId;
    /** @brief The path part of the request. */
    std::string mPath;
    /** @brief The parameters part of the query (without leading ?). */
    std::string mArgs;
    /** @brief The body part of the query */
    std::string mBody;
    /** @brief The query answer */
    std::string mAnswer;
    /** @brief The request uri */
    std::string mRequest;
    /** @brief The header part of the query */
    mapStr mReqHeader;
    /** @brief The header part of the query */
    std::string mReqBody;
    /** @brief The header part of the answer */
    mapStr  mResponseHeader;
    /** @brief The body part of the answer */
    std::string mResponseBody;
    /** @brief The header part of the answer of the duplicated request */
    mapStr mDupResponseHeader;
    /** @brief The body part of the answer of the duplicated request*/
    std::string mDupResponseBody;
    /** @brief The header part of the answer of the Compare module */
    mapStr mCompResponseHeader;
    /** @brief The Curl response status sent by Duplicate module */
    int mCurlResponseStatus;

    typedef std::list<std::pair<std::string, std::string> > tHeaders;

    /** @brief list that represents the headers of the incoming request */
    tHeaders mHeadersIn;

    /** @brief list that represents the headers of the request answer */
    tHeaders mHeadersOut;

    unsigned int offset;

    /* @brief The HTTP status provided by X_DUP_HTTP_STATUS header */
    int mReqHttpStatus;
    /* @brief The HTTP status returned by the duplicated request response */
    int mDupResponseHttpStatus;

    /** @brief true if X_DUP_LOG header is present in the request */
    bool mValidationHeaderDup;
    /** @brief true if X_DUP_LOG header is present in the request and the duplication type is whith answer */
    bool mValidationHeaderComp;

    /** @brief The conf object for the location of this Request */
    void * mConf;

    /**
     * @brief Constructs the object using the three strings.
     * @param id The query unique ID
     * @param pConfPath The location (in the conf) which matched this query
     * @param pPath The path part of the requests
     * @param pArgs The parameters part of the query (without leading ?)
     * @param body The body part of the query
     */
    RequestInfo(std::string id, const std::string &pConfPath, const std::string &pPath,
            const std::string &pArgs, const std::string *body = 0);

    /**
     * @brief Constructor dedicated for serialization purpose
     */
    RequestInfo(const mapStr &reqHeader, const std::string &reqBody, const mapStr &respHeader,
                const std::string &respBody, const mapStr &dupHeader, const std::string &dupBody);

    /**
     * @brief Constructs a request initialising it's id and start time
     * @param id unique Id
     * @param startTime WebServer Request Start Time in microseconds
     */
    RequestInfo(const std::string &id, int64_t startTime);

    /**
     * @brief Constructs a poisonous object causing the processor to stop when read
     */
    RequestInfo();

    /**
     * @brief Returns true if the request has a body
     */
    bool hasBody() const;

    /**
     * @brief Returns wether the the request is poisonous
     * @return true if poisonous, false otherwhise
     */
    bool isPoison() const;

    /**
     * @brief Formats the string toSerialize using the format
     * size on 8 bytes + value
     * e.g. value => 00000005value
     * content is appended to output
     */
    static void Serialize(const std::string &toSerialize, std::stringstream &output);


    /**
     * @brief Getter of the EOS flag indicator
     */
    bool eos_seen() const {
        return mEOS;
    }

    /**
     * @brief Sets the EOS flag
     * Computes the elapesed time on first call
     */
    void eos_seen(bool valToSet);


    /**
     * @brief Returns the computed elapsed time in MS
     */
    int getElapsedTimeMS() const;


    /**
     * @brief Reset the startTime to NOW
     */
    void resetStartTime() { mStartTime = boost::posix_time::microsec_clock::universal_time(); }

    /**
     * @brief Add time period to start time (used to not take into account )
     */
    //void addTimeToStartTime() { mStartTime = boost::posix_time::microsec_clock::universal_time(); }

private:

    /* End Of Stream marker */
    bool mEOS;

    /*
     * Initialisation of this struct time
     * Matches the start time of the apache handler
     */
    boost::posix_time::ptime mStartTime;
    boost::posix_time::time_duration mElapsedTime; /* Elapsed time by the handler to process the request */


};
}
