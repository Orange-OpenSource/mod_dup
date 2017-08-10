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

#include <boost/regex.hpp>
#include <boost/scoped_ptr.hpp>
#include <curl/curl.h>
#include <string>
#include <map>
#include <apr_pools.h>

#include "MultiThreadQueue.hh"
#include "RequestInfo.hh"
#include "UrlCodec.hh"
#include "RequestCommon.hh"


typedef void CURL;
struct request_rec;

class TestRequestProcessor;
class TestModDup;

namespace DupModule {
    
    class DupConf;

/**
 * Base class for filters and substitutions
 */
class tElementBase{
public:
    tElementBase(const std::string &regex,
            ApplicationScope::eApplicationScope scope);

    tElementBase(const std::string &regex,
            boost::regex::flag_type flags,
            ApplicationScope::eApplicationScope scope);

    tElementBase(const tElementBase &other);

    virtual ~tElementBase();

    ApplicationScope::eApplicationScope mScope;     /** The action of the filter */
    boost::regex mRegex;                            /** The matching regular expression */
};

/**
 * Represents a filter that applies on a key
 */
class tFilter : public tElementBase{
public:

    enum eFilterTypes {
        REGULAR,
        PREVENT_DUPLICATION,
    };

    tFilter(const std::string &regex,
            ApplicationScope::eApplicationScope scope,
            const std::string &currentDupDestination,
            DuplicationType::eDuplicationType dupType,
            tFilter::eFilterTypes fType = eFilterTypes::REGULAR);

    virtual ~tFilter();

    std::string mDestination;                               /** The host to duplicate the request to if the filter matches
                                                                    the destination in &lt;host>[:&lt;port>] format */
    DuplicationType::eDuplicationType mDuplicationType;     /** The duplication type for this filter */

    eFilterTypes mFilterType;

    mutable std::string mMatch;
};

/**
 * Represents a raw substitution
 */
class tSubstitute : public tElementBase{
public:
    tSubstitute(const std::string &regex,
            const std::string &replacement,
            ApplicationScope::eApplicationScope scope);

    virtual ~tSubstitute();

    std::string mReplacement; /** The replacement value regex */
};

/** @brief Maps a path to a substitution. Not a multimap because order matters. */
typedef std::map<std::string, std::list<tSubstitute> > tFieldSubstitutionMap;



/** @brief A container for the operations */
class Commands {
public:

    /**
     * @brief Default Ctor
     */
    Commands() : mDuplicationPercentage(100) {
    }

    /** @brief The list of filter commands
     * Indexed by the field on which they apply
     */
    std::multimap<std::string, tFilter> mFilters;

    /** @brief The Raw filter list */
    std::list<tFilter> mRawFilters;

    /** @brief The substition maps */
    tFieldSubstitutionMap mSubstitutions;

    /** @brief The Raw Substitution list */
    std::list<tSubstitute> mRawSubstitutions;

    /** The percentage of matching requests to duplicate */
    unsigned int mDuplicationPercentage;

    /**
     * @brief Returns true if the request must be duplicated
     * Uses the percentage of duplication to determine if the request must be
     * duplicated or not
     */
    bool toDuplicate();
};

/**
 * @brief RequestProcessor is responsible for processing and sending requests to their destination.
 * This is where all the business logic is configured and executed.
 * Its main method is run which will continuously pull requests off the internal queue in order to process them.
 */
class RequestProcessor
{
public:
    /** Commands indexed by the duplication destination*/
    typedef std::map<std::string, Commands> tCommandsByDestination;
    typedef std::map<const void *, tCommandsByDestination> tCommandsByConfPathAndDestination;

private:
    /** @brief Maps DupConf(per virtualhost+path)+Destination to their corresponding processing (filter and substitution) directives */
    tCommandsByConfPathAndDestination mCommands;

    /** @brief The timeout for outgoing requests in ms */
    unsigned int                                    mTimeout;

    /** @brief The number of requests which timed out */
    volatile unsigned int                           mTimeoutCount;

    /** @brief The number of requests duplicated */
    volatile unsigned int                           mDuplicatedCount;

    /** @brief The codec to use when encoding the url*/
    boost::scoped_ptr<const IUrlCodec>              mUrlCodec;

    static void addOrigHeaders(const RequestInfo &rInfo, curl_slist *&slist);
    static void addCommonHeaders(const RequestInfo &rInfo, curl_slist *&slist);
    static void addValidationHeadersCompare(RequestInfo &rInfo, const tFilter &matchedFilter, curl_slist *&slist);
    static void addValidationHeadersDup(RequestInfo &rInfo, const std::list<const tFilter *> & matchedFilters, int numDestinations, int numFiltersAttempted);

    void
    sendInBody(CURL *curl, const RequestInfo &rInfo, curl_slist *&slist, const std::string &toSend) const;

    std::string *
    sendDupFormat(CURL *curl, const RequestInfo &rInfo, curl_slist *&slist) const;

public:
    /**
     * @brief Constructs a RequestProcessor
     */
    RequestProcessor();

    /**
     * @brief Set the timeout
     * @param pTimeout the timeout in ms
     */
    void
    setTimeout(const unsigned int &pTimeout);

    /**
     * @brief Get the number of requests which timed out since last call to this method
     * @return The timeout count
     */
    const unsigned int
    getTimeoutCount();

    /**
     * @brief Get the number of requests duplicated since last call to this method
     * @return The duplicated count
     */
    const unsigned int
    getDuplicatedCount();

    /**
     * @brief Set the url codec
     * @param pUrlCodec the codec to use
     */
    void
    setUrlCodec(const std::string &pUrlCodec="default");

    /**
     * @brief Add a filter for all requests on a given path
     * @param pPath the path of the request
     * @param pField the field on which to do the substitution
     * @param pFilter a reg exp which has to match for this request to be duplicated
     * @param pAssociatedConf the filter declaration context
     * @param fType the filter type
     */
    void
    addFilter(const std::string &pField, const std::string &pFilter,
            const DupConf &pAssociatedConf, tFilter::eFilterTypes fType);

    /**
     * @brief Sets a destination duplication percentage
     * @param pPath the path of the request
     * @param destination : the destination to treat
     * @param percentage : the percentage to affect to this destination
     */
    void
    setDestinationDuplicationPercentage(const DupConf &pAssociatedConf, const std::string &destination,
                                        int percentage);

    /**
     * @brief Add a RAW filter for all requests on a given path
     * @param pPath the path of the request
     * @param pFilter a reg exp which has to match for this request to be duplicated
     * @param pAssociatedConf the filter declaration context
     * @param fType the filter type
     */
    void
    addRawFilter(const std::string &pFilter,
            const DupConf &pAssociatedConf, tFilter::eFilterTypes fType);

    /**
     * @brief Schedule a substitution on the value of a given field of all requests on a given path
     * @param pPath the path of the request
     * @param pField the field on which to do the substitution
     * @param pMatch the regexp matching what should be replaced
     * @param pReplace the value which the match should be replaced with
     * @param pAssociatedConf the filter declaration context
     */
    void
    addSubstitution(const std::string &pField,
            const std::string &pMatch, const std::string &pReplace,
            const DupConf &pAssociatedConf);

    /**
     * @brief Schedule a Raw substitution on the value of all requests on a given path
     * @param pPath the path of the request
     * @param pField the field on which to do the substitution
     * @param pMatch the regexp matching what should be replaced
     * @param pReplace the value which the match should be replaced with
     * @param pAssociatedConf the filter declaration context
     */
    void
    addRawSubstitution(const std::string &pMatch, const std::string &pReplace,
            const DupConf &pAssociatedConf);

    /**
     * @brief Returns wether or not the request matches any of the filters
     * @param pRequest the incoming request
     * @param pCommands the commands to apply
     * @param pParsedArgs the list filled with the key value pairs to be matched
     * @return the first matched filter, null otherwise
     */
    const tFilter*
    matchesFilter(RequestInfo &pRequest, const Commands &pCommands);

    /**
     * @brief Parses arguments into key valye pairs. Also url-decodes values and converts keys to upper case.
     * @param pParsedArgs the list which should be filled with the key value pairs
     * @param pArgs the parameters part of the query
     */
    void
    parseArgs(tKeyValList &pParsedArgs, const std::string &pArgs);

    /**
     * @brief Process a field. This includes filtering and executing substitutions
     * @param pRequest the path of the configuration which is applied
     * @return an empty list if the request does not need to be duplicated, a filter by duplication that matched otherwise.
     */
    std::list<const tFilter *>
    processRequest(RequestInfo &pRequest);

    /**
     * @brief Run the infinite loop which pops new requests of the given queue, processes them and sends the over to the configured destination
     * @param pQueue the queue which gets filled with incoming requests
     */
    void
    run(MultiThreadQueue<boost::shared_ptr<RequestInfo> > &pQueue);

    /**
     * @brief initialize curl handle and common curl options
     * @return a curl handle
     */
    CURL * initCurl();

    void
    performCurlCall(CURL *curl, const tFilter &matchedFilter, RequestInfo &rInfo);

    /**
     * @brief perform curl for one request if it matches
     * @param reqInfo the RequestInfo instance for this request
     * @param pCurl a preinitialized curl handle
     */
    void runOne(RequestInfo &reqInfo, CURL * pCurl);

private:

    bool
    substituteRequest(RequestInfo &pRequest, Commands &pCommands);

    const tFilter *
    keyFilterMatch(const std::multimap<std::string, tFilter> &pFilters, const tKeyValList &pParsedArgs,
            ApplicationScope::eApplicationScope scope, tFilter::eFilterTypes eType);

    bool
    keySubstitute(tFieldSubstitutionMap &pSubs,
            tKeyValList &pParsedArgs,
            ApplicationScope::eApplicationScope scope,
            std::string &result);
    bool
    headerSubstitute(tFieldSubstitutionMap &pSubs,
                     tKeyValList &pHeadersIn);

    friend class ::TestRequestProcessor;
    friend class ::TestModDup;

};


}
