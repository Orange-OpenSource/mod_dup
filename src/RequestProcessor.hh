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


typedef void CURL;
struct request_rec;

class TestRequestProcessor;
class TestContextEnrichment;

namespace DupModule {

    class DupConf;

    typedef std::pair<std::string, std::string> tKeyVal;

    namespace ApplicationScope {

        /**
         * Scopes that a filter/sub can have
         */
        enum eApplicationScope{
            ALL = 0x3,
            HEADER = 0x1,
            BODY = 0x2,
        };
        extern const char* c_ALL;
        extern const char* c_HEADER;
        extern const char* c_BODY;
        extern const char* c_ERROR_ON_STRING_VALUE;

        /**
         * Translates the character value of a scope into it's enumerate value
         * raises a std::exception if the string doesn't match any predefined values
         * Values are : ALL, BODY, HEADER
         */
        eApplicationScope stringToEnum(const char* strValue) throw (std::exception);

    };

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

    /**
     * Context enrichment bean
     */
    class tContextEnrichment : public tElementBase{
    public:
        tContextEnrichment(const std::string &varName,
                           const std::string &matchregex,
                           const std::string &setValue,
                           ApplicationScope::eApplicationScope scope);

        virtual ~tContextEnrichment();

        std::string mVarName;   /** The variable name to set if it matches */
        std::string mSetValue;  /** The value to set if it matches */
    };


    /** @brief Maps a path to a substitution. Not a multimap because order matters. */
    typedef std::map<std::string, std::list<tSubstitute> > tFieldSubstitutionMap;


    /** @brief A container for the filter and substituion commands */
    class tRequestProcessorCommands {
    public:
        /** @brief The list of filter commands
         * Indexed by the field on which they apply
         */
        std::multimap<std::string, tFilter> mFilters;

        /** @brief The substition maps */
        tFieldSubstitutionMap mSubstitutions;

        /** @brief The Raw filter list */
        std::list<tFilter> mRawFilters;

        /** @brief The Raw Substitution list */
        std::list<tSubstitute> mRawSubstitutions;

        /** @brief The Context enrichment list */
        std::list<tContextEnrichment> mEnrichContext;

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
	std::map<std::string, tRequestProcessorCommands> mCommands;

	/** @brief The timeout for outgoing requests in ms */
	unsigned int                                    mTimeout;

	/** @brief The number of requests which timed out */
	volatile unsigned int                           mTimeoutCount;

        /** @brief The number of requests duplicated */
        volatile unsigned int                           mDuplicatedCount;

        /** @brief The url codec */
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
         * @brief EnrichContext instructions
         * @param pPath the path of the request
         * @param pVarName the name of the variable to declare
         * @param pMatch the regexp that must match to declare the variable
         * Regex scope if defined in the DupConf struct
         * @param pSetValue the value to set to the variable if the regex matches
         */
        void
        addEnrichContext(const std::string &pPath, const std::string &pVarName,
                         const std::string &pMatch, const std::string &pSetValue,
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
        argsMatchFilter(RequestInfo &pRequest, tRequestProcessorCommands &pCommands, std::list<tKeyVal> &pParsedArgs);

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
         * @return NULL if the request does not need to be duplicated, a pointer on the filter that matched otherwise.
         * If and only if a filter matches, substitutions will be applied.
         */
        const tFilter *
        processRequest(RequestInfo &pRequest);

        /**
         * @brief Run the infinite loop which pops new requests of the given queue, processes them and sends the over to the configured destination
         * @param pQueue the queue which gets filled with incoming requests
         */
        void
        run(MultiThreadQueue<boost::shared_ptr<RequestInfo> > &pQueue);

        /**
         * @brief perform curl for one request if it matches
         * @param reqInfo the RequestInfo instance for this request
         * @param pCurl a preinitialized curl handle
         */
        void runOne(RequestInfo &reqInfo, CURL * pCurl);

        /**
         * @brief initialize curl handle and common curl options
         * @return a curl handle
         */
        CURL * initCurl();

        /**
         * @brief Define some environnement variables if the query matches the criteria defined
         * using the DupEnrichContext directive
         * @param pRequest the apache request structure
         * @param the RequestInfo internal struct containing the body
         * @return the number of variables defined
         */
        int
        enrichContext(request_rec *pRequest, const RequestInfo &rInfo);

    private:

        bool
        substituteRequest(RequestInfo &pRequest, tRequestProcessorCommands &pCommands,
                          std::list<tKeyVal> &pHeaderParsedArgs);

        const tFilter *
        keyFilterMatch(std::multimap<std::string, tFilter> &pFilters, const std::list<tKeyVal> &pParsedArgs,
                       ApplicationScope::eApplicationScope scope, tFilter::eFilterTypes eType);

        bool
        keySubstitute(tFieldSubstitutionMap &pSubs,
                      std::list<tKeyVal> &pParsedArgs,
                      ApplicationScope::eApplicationScope scope,
                      std::string &result);

        void
        performCurlCall(CURL *curl, const tFilter &matchedFilter, const RequestInfo &rInfo);

        friend class ::TestRequestProcessor;
        friend class ::TestContextEnrichment;
    };


}
