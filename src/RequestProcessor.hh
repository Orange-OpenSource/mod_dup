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

namespace DupModule {

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

    std::string mField;                                     /** The key or field the filter applies on */
    std::string mDestination;                               /** The host to duplicate the request to if the filter matches
                                                                    the destination in &lt;host>[:&lt;port>] format */
    DuplicationType::eDuplicationType mDuplicationType;     /** The duplication type for this filter */

    eFilterTypes mFilterType;
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
};

/**
 * @brief Overlay on the commands object
 * Adds a destination concept
 */
struct CommandsByDestination {

    /** Commands indexed by the duplication destination*/
    std::map<std::string, Commands> mCommands;

    /**
     * @brief Returns true if we have to duplicate to this destination
     * Means that at least, one of it's filters matches
     */
    bool toDuplicate() const;
};

/**
 * @brief RequestProcessor is responsible for processing and sending requests to their destination.
 * This is where all the business logic is configured and executed.
 * Its main method is run which will continuously pull requests off the internal queue in order to process them.
 */
class RequestProcessor
{

private:
    /** @brief Maps paths to their corresponding processing (filter and substitution) directives */
    std::map<std::string, CommandsByDestination> mCommands;

    /** @brief The timeout for outgoing requests in ms */
    unsigned int                                    mTimeout;

    /** @brief The number of requests which timed out */
    volatile unsigned int                           mTimeoutCount;

    /** @brief The number of requests duplicated */
    volatile unsigned int                           mDuplicatedCount;

    /** @brief The codec to use when encoding the url*/
    boost::scoped_ptr<const IUrlCodec>              mUrlCodec;

    void
    sendInBody(CURL *curl, curl_slist *&slist, const std::string &toSend) const;

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
     */
    void
    addFilter(const std::string &pPath, const std::string &pField, const std::string &pFilter,
            const DupConf &pAssociatedConf, tFilter::eFilterTypes fType);

    /**
     * @brief Add a RAW filter for all requests on a given path
     * @param pPath the path of the request
     * @param pFilter a reg exp which has to match for this request to be duplicated
     * @param Scope: the elements to match the filter with
     */
    void
    addRawFilter(const std::string &pPath, const std::string &pFilter,
            const DupConf &pAssociatedConf, tFilter::eFilterTypes fType);

    /**
     * @brief Schedule a substitution on the value of a given field of all requests on a given path
     * @param pPath the path of the request
     * @param pField the field on which to do the substitution
     * @param pMatch the regexp matching what should be replaced
     * @param pReplace the value which the match should be replaced with
     */
    void
    addSubstitution(const std::string &pPath, const std::string &pField,
            const std::string &pMatch, const std::string &pReplace,
            const DupConf &pAssociatedConf);

    /**
     * @brief Schedule a Raw substitution on the value of all requests on a given path
     * @param pPath the path of the request
     * @param pField the field on which to do the substitution
     * @param pMatch the regexp matching what should be replaced
     * @param pReplace the value which the match should be replaced with
     */
    void
    addRawSubstitution(const std::string &pPath, const std::string &pMatch, const std::string &pReplace,
            const DupConf &pAssociatedConf);

    /**
     * @brief Returns wether or not the arguments match any of the filters
     * @param pParsedArgs the list with the argument key value pairs
     * @param pFilters the filters which should be applied
     * @return true if there are no filters or at least one filter matches, false otherwhise
     */
    const tFilter*
    argsMatchFilter(RequestInfo &pRequest, Commands &pCommands, std::list<tKeyVal> &pParsedArgs);

    /**
     * @brief Parses arguments into key valye pairs. Also url-decodes values and converts keys to upper case.
     * @param pParsedArgs the list which should be filled with the key value pairs
     * @param pArgs the parameters part of the query
     */
    void
    parseArgs(std::list<tKeyVal> &pParsedArgs, const std::string &pArgs);

    /**
     * @brief Process a field. This includes filtering and executing substitutions
     * @param pConfPath the path of the configuration which is applied
     * @param pArgs the HTTP arguments/parameters of the incoming request
     * @return an empty list if the request does not need to be duplicated, a filter by duplication that matched otherwise.
     */
    std::list<const tFilter *>
    processRequest(RequestInfo &pRequest, std::list<std::pair<std::string, std::string> > parsedArgs);

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
    performCurlCall(CURL *curl, const tFilter &matchedFilter, const RequestInfo &rInfo);

    /**
     * @brief perform curl for one request if it matches
     * @param reqInfo the RequestInfo instance for this request
     * @param pCurl a preinitialized curl handle
     */
    void runOne(RequestInfo &reqInfo, CURL * pCurl);

private:

    bool
    substituteRequest(RequestInfo &pRequest, Commands &pCommands,
            std::list<tKeyVal> &pHeaderParsedArgs);

    const tFilter *
    keyFilterMatch(std::multimap<std::string, tFilter> &pFilters, const std::list<tKeyVal> &pParsedArgs,
            ApplicationScope::eApplicationScope scope, tFilter::eFilterTypes eType);

    bool
    keySubstitute(tFieldSubstitutionMap &pSubs,
            std::list<tKeyVal> &pParsedArgs,
            ApplicationScope::eApplicationScope scope,
            std::string &result);

    friend class ::TestRequestProcessor;
};


}
