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

#include <boost/foreach.hpp>
#include <curl/curl.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <httpd.h>

#include "RequestProcessor.hh"

namespace DupModule {

const char * gUserAgent = "mod-dup";

/**
 * @brief Set the destination server and port
 * @param pDestination the destination in &lt;host>[:&lt;port>] format
 */
void
RequestProcessor::setDestination(const std::string &pDestination) {
	mDestination = pDestination;
}

/**
 * @brief Set the timeout
 * @param pTimeout the timeout in ms
 */
void
RequestProcessor::setTimeout(const unsigned int &pTimeout) {
	mTimeout = pTimeout;
}

bool
RequestInfo::hasBody() const {
    return mBody.size();
}

/**
 * @brief Get the number of requests which timed out since last call to this method
 * @return The timeout count
 */
const unsigned int
RequestProcessor::getTimeoutCount() {
	// Atomic read + reset
	// Works because mTimeoutCount & 0 == 0
	unsigned int lTimeoutCount = __sync_fetch_and_and(&mTimeoutCount, 0);
	if (lTimeoutCount > 0) {
		Log::warn(303, "%u requests timed out during last cycle!", lTimeoutCount);
	}
	return lTimeoutCount;
}

/**
 * @brief Get the number of requests that were duplicated since last call to this method
 * @return The duplicated count
 */
const unsigned int
RequestProcessor::getDuplicatedCount() {
    // Atomic read + reset
    // Works because mDuplicatedCount & 0 == 0
    unsigned int lCount = __sync_fetch_and_and(&mDuplicatedCount, 0);
    return lCount;
}

/**
 * @brief Add a filter for all requests on a given path
 * @param pPath the path of the request
 * @param pField the field on which to do the substitution
 * @param pFilter a reg exp which has to match for this request to be duplicated
 */
void
RequestProcessor::addFilter(const std::string &pPath, const std::string &pField, const std::string &pFilter, tFilterBase::eFilterScope scope) {

    mCommands[pPath].mFilters.insert(std::pair<std::string, tFilter>(boost::to_upper_copy(pField),
                                                                     tFilter(pFilter, scope)));
}

void
RequestProcessor::addRawFilter(const std::string &pPath, const std::string &pFilter, tFilterBase::eFilterScope scope) {
    mCommands[pPath].mRawFilters.push_back(tFilter(pFilter, scope));
}

/**
 * @brief Schedule a substitution on the value of a given field of all requests on a given path
 * @param pPath the path of the request
 * @param pField the field on which to do the substitution
 * @param pMatch the regexp matching what should be replaced
 * @param pReplace the value which the match should be replaced with
 */
void
RequestProcessor::addSubstitution(const std::string &pPath, const std::string &pField, const std::string &pMatch,
                                  const std::string &pReplace, tFilterBase::eFilterScope scope) {
    mCommands[pPath].mSubstitutions[boost::to_upper_copy(pField)].push_back(tSubstitute(pMatch, pReplace, scope));
}

void
RequestProcessor::addRawSubstitution(const std::string &pPath, const std::string &pRegex, const std::string &pReplace, tFilterBase::eFilterScope pScope){
    mCommands[pPath].mRawSubstitutions.push_back(tSubstitute(pRegex, pReplace, pScope));
}

/**
 * @brief Parses arguments into key valye pairs. Also url-decodes values and converts keys to upper case.
 * @param pParsedArgs the list which should be filled with the key value pairs
 * @param pArgs the parameters part of the query
 */
void
RequestProcessor::parseArgs(std::list<tKeyVal> &pParsedArgs, const std::string &pArgs) {
    const boost::char_separator<char> lSep("&");
    boost::tokenizer<boost::char_separator<char> > lTokens(pArgs, lSep);
    BOOST_FOREACH (const std::string &lToken, lTokens) {
        size_t lEqualPos = lToken.find('=');
        if (lEqualPos == std::string::npos) {
            std::string lKey = boost::to_upper_copy(lToken);

            pParsedArgs.push_back(tKeyVal(lKey, std::string()));
        } else {
            std::string lKey = lToken.substr(0, lEqualPos);
            boost::to_upper(lKey);
            std::string lVal = mUrlCodec->decode(lToken.substr(lEqualPos+1, std::string::npos));
            pParsedArgs.push_back(tKeyVal(lKey, lVal));
        }
    }
}

bool
RequestProcessor::keyFilterMatch(std::multimap<std::string, tFilter> &pFilters, std::list<tKeyVal> &pParsedArgs, tFilterBase::eFilterScope scope){
    // Key filter matching
    BOOST_FOREACH (const tKeyVal lKeyVal, pParsedArgs) {
        // Key Iteration
        std::pair<std::multimap<std::string, tFilter>::iterator,
                  std::multimap<std::string, tFilter>::iterator> lFilterIter = pFilters.equal_range(lKeyVal.first);
        // FilterIteration
        for (std::multimap<std::string, tFilter>::iterator it = lFilterIter.first; it != lFilterIter.second; ++it) {
            if ((it->second.mScope & scope) &&                                  // Scope check
                boost::regex_search(lKeyVal.second, it->second.mRegex)) {        // Regex match
                Log::debug("Key filter matched: %s | %s", lKeyVal.second.c_str(), it->second.mRegex.str().c_str());
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Returns wether or not the arguments match any of the filters
 * @param pParsedArgs the list with the argument key value pairs
 * @param pFilters the filters which should be applied
 * @return true if there are no filters or at least one filter matches, false otherwhise
 */
bool
RequestProcessor::argsMatchFilter(RequestInfo &pRequest, tRequestProcessorCommands &pCommands, std::list<tKeyVal> &pHeaderParsedArgs) {

    std::multimap<std::string, tFilter> &pFilters = pCommands.mFilters;
    std::list<tFilter> &pRawFilters = pCommands.mRawFilters;

    // If no filter is defined, we accept all queries
    if (pFilters.empty() && pRawFilters.empty()) {
        return true;
    }

    // Key filter type detection
    bool keyFilterOnHeader = false;
    bool keyFilterOnBody = false;
    typedef std::pair<const std::string, tFilter> value_type;
    BOOST_FOREACH(value_type &f, pFilters) {
        if (f.second.mScope & tFilterBase::BODY)
            keyFilterOnBody = true;
        if (f.second.mScope & tFilterBase::HEADER)
            keyFilterOnHeader = true;
     }

    // Key filters on header
    if (keyFilterOnHeader && keyFilterMatch(pFilters, pHeaderParsedArgs, tFilterBase::HEADER)){
        return true;
    }

    // Key filters on body
    if (keyFilterOnBody){
        std::list<tKeyVal> lParsedArgs;
        parseArgs(lParsedArgs, pRequest.mBody);
        if (keyFilterMatch(pFilters, lParsedArgs, tFilterBase::BODY))
            return true;
    }

    // Raw filters matching
    BOOST_FOREACH (tFilter &raw, pCommands.mRawFilters) {
        // Header application
        if (raw.mScope & tFilterBase::HEADER) {
            if (boost::regex_search(pRequest.mArgs, raw.mRegex)) {
                Log::debug("Raw filter (HEADER) matched: %s | %s", pRequest.mArgs.c_str(), raw.mRegex.str().c_str());
                return true;
            }
        }
        // Body application
        if (raw.mScope & tFilterBase::BODY) {
            if (boost::regex_search(pRequest.mBody, raw.mRegex)) {
                Log::debug("Raw filter (BODY) matched: %s | %s", pRequest.mBody.c_str(), raw.mRegex.str().c_str());
                return true;
            }
        }
    }
    return false;
}

bool
RequestProcessor::keySubstitute(tFieldSubstitutionMap &pSubs,
                                std::list<tKeyVal> &pParsedArgs,
                                tFilterBase::eFilterScope scope,
                                std::string &result){
    apr_pool_t *lPool = NULL;
    apr_pool_create(&lPool, 0);

    std::list<std::string> lNewArgs;
    bool lDidSubstitute = false;

    // Run through the keys
    BOOST_FOREACH (const tKeyVal lKeyVal, pParsedArgs) {
        std::map<std::string, std::list<tSubstitute> >::iterator lSubstIter = pSubs.find(lKeyVal.first);
        std::string lVal = lKeyVal.second;

        // Key found in the subs?
        if (lSubstIter != pSubs.end()) {
            BOOST_FOREACH(const tSubstitute &lSubst, lSubstIter->second) {
                Log::debug("Key substitute: %d | lVal:%s | lSubst:%s | Rep:%s", (int) lSubst.mScope, lVal.c_str(),
                           lSubst.mRegex.str().c_str(), lSubst.mReplacement.c_str());
                if (!(scope & lSubst.mScope))
                    continue;

                lVal = boost::regex_replace(lVal, lSubst.mRegex, lSubst.mReplacement, boost::match_default | boost::format_all);
                lDidSubstitute = true;
                Log::debug("Key substitute res: lVal:%s ", lVal.c_str());

            }
        }
        if (lVal.empty()) {
            lNewArgs.push_back(lKeyVal.first);
        } else {
            lNewArgs.push_back(lKeyVal.first + "=" + mUrlCodec->encode(lPool, lVal));
        }
    }
    if (lDidSubstitute) {
        result = boost::algorithm::join(lNewArgs, "&");
    }
    apr_pool_destroy(lPool);
    return lDidSubstitute;
}

bool
RequestProcessor::substituteRequest(RequestInfo &pRequest, tRequestProcessorCommands &pCommands, std::list<tKeyVal> &pHeaderParsedArgs) {
    // Ideally we would use the pool from the apache request, but it's used in another thread

    bool keySubOnBody = false, keySubOnHeader = false;

    // Detect the presence of the different key filters
    typedef std::pair<const std::string, std::list<tSubstitute> > value_type;
    BOOST_FOREACH(value_type &f, pCommands.mSubstitutions) {
        BOOST_FOREACH(tSubstitute &s, f.second) {
            if (s.mScope & tFilterBase::BODY)
                keySubOnBody = true;
            if (s.mScope & tFilterBase::HEADER)
                keySubOnHeader = true;
        }
     }

    bool lDidSubstitute = false;
    // Perform the key substitutions
    if (keySubOnHeader) {
        // On the header
        lDidSubstitute = keySubstitute(pCommands.mSubstitutions,
                                       pHeaderParsedArgs,
                                       tFilterBase::HEADER,
                                       pRequest.mArgs);
    }
    if (keySubOnBody) {
        // On the body
        std::list<tKeyVal> lParsedArgs;
        parseArgs(lParsedArgs, pRequest.mBody);
        lDidSubstitute |= keySubstitute(pCommands.mSubstitutions,
                                       lParsedArgs,
                                       tFilterBase::BODY,
                                       pRequest.mBody);
    }
    // Run the raw substitutions
    BOOST_FOREACH(tSubstitute &s, pCommands.mRawSubstitutions) {
        if (s.mScope & tFilterBase::BODY) {
            pRequest.mBody = boost::regex_replace(pRequest.mBody, s.mRegex, s.mReplacement, boost::match_default | boost::format_all);
        }
        if (s.mScope & tFilterBase::HEADER) {
            pRequest.mArgs = boost::regex_replace(pRequest.mArgs, s.mRegex, s.mReplacement, boost::match_default | boost::format_all);
        }
        lDidSubstitute = true;
    }
    return lDidSubstitute;
}

/**
 * @brief Process a field. This includes filtering and executing substitutions
 * Substitutions are applied on individual fields whereas filters are applied on the whole parameter string.
 * Before any processing is applied, the parameter string is url decoded.
 * After all processing the values of each field get url encoded again.
 * @param pConfPath the path of the configuration which is applied
 * @param pArgs the HTTP arguments/parameters of the incoming request
 * @return true if the request should get duplicated, false otherwise.
 * If and only if it returned true, pArgs will have all necessary substitutions applied.
 */
bool
RequestProcessor::processRequest(const std::string &pConfPath, RequestInfo &pRequest) {
    std::map<std::string, tRequestProcessorCommands>::iterator it = mCommands.find(pConfPath);
    if (it == mCommands.end()) {
        // Filtering on paths is done by Apache, so if we don't know about this path,
        // it only means that no processing is needed - we can duplicate it straight away
        return true;
    }

    tRequestProcessorCommands lCommands = (*it).second;

    std::list<std::pair<std::string, std::string> > lParsedArgs;
    parseArgs(lParsedArgs, pRequest.mArgs);

    // Tests if at least one acitve filter matches
    if (!argsMatchFilter(pRequest, lCommands, lParsedArgs)) {
		Log::debug("No args match filter");
        return false;
    }

    Log::debug("Filter match");

    // We have a match, perform substitutions
    substituteRequest(pRequest, lCommands, lParsedArgs);
    return true;
}

void
RequestProcessor::setUrlCodec(const std::string &pUrlCodec)
{
	mUrlCodec.reset(getUrlCodec(pUrlCodec));
}

/**
 * @brief Run the infinite loop which pops new requests of the given queue, processes them and sends the over to the configured destination
 * @param pQueue the queue which gets filled with incoming requests
 */
void
RequestProcessor::run(MultiThreadQueue<RequestInfo> &pQueue)
{
    Log::debug("New worker thread started");

    if (mDestination.empty()) {
        Log::error(401, "Configuration error. No duplication destination set.");
        return;
    }

    CURL * lCurl = curl_easy_init();
    if (!lCurl) {
        Log::error(402, "Could not init curl request object.");
        return;
    }
    curl_easy_setopt(lCurl, CURLOPT_USERAGENT, gUserAgent);
    // Activer l'option provoque des timeouts sur des requests avec un fort payload
    curl_easy_setopt(lCurl, CURLOPT_TIMEOUT_MS, mTimeout);
    curl_easy_setopt(lCurl, CURLOPT_NOSIGNAL, 1);

    for (;;) {
        RequestInfo lQueueItem = pQueue.pop();
        if (lQueueItem.isPoison()) {
            // Master tells us to stop
            Log::debug("Received poison pill. Exiting.");
            break;
        }
        if (processRequest(lQueueItem.mConfPath, lQueueItem)) {
            __sync_fetch_and_add(&mDuplicatedCount, 1);
            std::string request = mDestination + lQueueItem.mPath + "?" + lQueueItem.mArgs;
            curl_easy_setopt(lCurl, CURLOPT_URL, request.c_str());
            struct curl_slist *slist = NULL;
            if (lQueueItem.hasBody()) {
                Log::debug("Before post: %s", boost::lexical_cast<std::string>(lQueueItem.mBody.size()).c_str());

                slist = curl_slist_append(slist, "Content-Type: text/xml; charset=utf-8");
                // Avoid Expect: 100 continue
                slist = curl_slist_append(slist, "Expect:");
                std::string contentLen = std::string("Content-Length: ") +
                    boost::lexical_cast<std::string>(lQueueItem.mBody.size());
                curl_slist_append(slist, contentLen.c_str());
                curl_easy_setopt(lCurl, CURLOPT_POST, 1);
                curl_easy_setopt(lCurl, CURLOPT_HTTPHEADER, slist);
                curl_easy_setopt(lCurl, CURLOPT_POSTFIELDSIZE, lQueueItem.mBody.size());
                curl_easy_setopt(lCurl, CURLOPT_POSTFIELDS, lQueueItem.mBody.c_str());
            } else {
                curl_easy_setopt(lCurl, CURLOPT_HTTPGET, 1);
                curl_easy_setopt(lCurl, CURLOPT_HTTPHEADER, NULL);
            }

            Log::debug("Duplicating: %s", request.c_str());

            int err = curl_easy_perform(lCurl);
            if (slist)
                curl_slist_free_all(slist);
            if (err == CURLE_OPERATION_TIMEDOUT) {
                __sync_fetch_and_add(&mTimeoutCount, 1);
            } else if (err) {
                Log::error(403, "Sending request failed with curl error code: %d, request:%s", err, request.c_str());
            }
        }
    }
    curl_easy_cleanup(lCurl);
}

tFilterBase::tFilterBase(const std::string &r, eFilterScope s)
    : mScope(s)
    , mRegex(r) {
}

tFilter::tFilter(const std::string &regex, eFilterScope scope)
    : tFilterBase(regex, scope) {
}

tFilterBase::eFilterScope tFilterBase::GetScopeFromString(const char *str) {
    if (!strcmp(str, "ALL"))
        return ALL;
    if (!strcmp(str, "HEADER"))
        return HEADER;
    if (!strcmp(str, "BODY"))
        return BODY;
    throw std::exception();
}

tSubstitute::tSubstitute(const std::string &regex, const std::string &replacement, eFilterScope scope)
    : tFilterBase(regex, scope)
    , mReplacement(replacement){
}

}
