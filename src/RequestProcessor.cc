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
#include "mod_dup.hh"

namespace DupModule {

const char * gUserAgent = "mod-dup";

namespace ApplicationScope {

      const char* c_ALL = "ALL";
      const char* c_BODY = "BODY";
      const char* c_HEADER = "HEADER";
      const char* c_ERROR_ON_STRING_VALUE = "Invalid ApplicationScope Value. Supported Values: ALL | HEADER | BODY" ;

    eApplicationScope stringToEnum(const char *str) throw (std::exception) {
        if (!strcmp(str, c_ALL))
            return ApplicationScope::ALL;
        if (!strcmp(str, c_HEADER))
            return ApplicationScope::HEADER;
        if (!strcmp(str, c_BODY))
            return ApplicationScope::BODY;
        throw std::exception();
    }
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
RequestProcessor::addFilter(const std::string &pPath, const std::string &pField, const std::string &pFilter,
                            const DupConf &pAssociatedConf) {

    mCommands[pPath].mFilters.insert(std::pair<std::string, tFilter>(boost::to_upper_copy(pField),
                                                                     tFilter(pFilter, pAssociatedConf.currentApplicationScope,
                                                                             pAssociatedConf.currentDupDestination)));
}

void
RequestProcessor::addRawFilter(const std::string &pPath, const std::string &pFilter,
                                const DupConf &pAssociatedConf) {
    mCommands[pPath].mRawFilters.push_back(tFilter(pFilter, pAssociatedConf.currentApplicationScope,
                                                   pAssociatedConf.currentDupDestination));
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
                                  const std::string &pReplace,  const DupConf &pAssociatedConf) {
    mCommands[pPath].mSubstitutions[boost::to_upper_copy(pField)].push_back(tSubstitute(pMatch, pReplace,
                                                                                        pAssociatedConf.currentApplicationScope));
}

void
RequestProcessor::addRawSubstitution(const std::string &pPath, const std::string &pRegex, const std::string &pReplace,
                                      const DupConf &pAssociatedConf){
    mCommands[pPath].mRawSubstitutions.push_back(tSubstitute(pRegex, pReplace,
                                                             pAssociatedConf.currentApplicationScope));
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

const tFilter *
RequestProcessor::keyFilterMatch(std::multimap<std::string, tFilter> &pFilters, std::list<tKeyVal> &pParsedArgs, ApplicationScope::eApplicationScope scope){
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
                return new tFilter(it->second);
            }
        }
    }
    return NULL;
}

/**
 * @brief Returns wether or not the arguments match any of the filters
 * @param pParsedArgs the list with the argument key value pairs
 * @param pFilters the filters which should be applied
 * @return a pointer on the filter if matches, NULL otherwise
 */
const tFilter *
RequestProcessor::argsMatchFilter(RequestInfo &pRequest, tRequestProcessorCommands &pCommands, std::list<tKeyVal> &pHeaderParsedArgs) {

    const tFilter *matched = NULL;
    std::multimap<std::string, tFilter> &pFilters = pCommands.mFilters;

    // Key filter type detection
    bool keyFilterOnHeader = false;
    bool keyFilterOnBody = false;
    typedef std::pair<const std::string, tFilter> value_type;
    BOOST_FOREACH(value_type &f, pFilters) {
        if (f.second.mScope & ApplicationScope::BODY)
            keyFilterOnBody = true;
        if (f.second.mScope & ApplicationScope::HEADER)
            keyFilterOnHeader = true;
     }

    Log::debug("Filters on body: %d | on header: %d", keyFilterOnBody, keyFilterOnHeader);

    // Key filters on header
    if (keyFilterOnHeader && (matched = keyFilterMatch(pFilters, pHeaderParsedArgs, ApplicationScope::HEADER))){
        Log::debug("Filter on HEADER match: destination:%s", matched->mDestination.c_str());
        return matched;
    }

    // Key filters on body
    if (keyFilterOnBody){
        std::list<tKeyVal> lParsedArgs;
        parseArgs(lParsedArgs, pRequest.mBody);
        if ((matched = keyFilterMatch(pFilters, lParsedArgs, ApplicationScope::BODY))) {
            Log::debug("Filter on BODY match: destination:%s", matched->mDestination.c_str());
            return matched;
        }
    }

    // Raw filters matching
    BOOST_FOREACH (tFilter &raw, pCommands.mRawFilters) {
        // Header application
        if (raw.mScope & ApplicationScope::HEADER) {
            if (boost::regex_search(pRequest.mArgs, raw.mRegex)) {
                Log::debug("Raw filter (HEADER) matched: %s | %s", pRequest.mArgs.c_str(), raw.mRegex.str().c_str());
                return &raw;
            }
        }
        // Body application
        if (raw.mScope & ApplicationScope::BODY) {
            if (boost::regex_search(pRequest.mBody, raw.mRegex)) {
                Log::debug("Raw filter (BODY) matched: %s | %s", pRequest.mBody.c_str(), raw.mRegex.str().c_str());
                return &raw;
            }
        }
    }
    return NULL;
}

bool
RequestProcessor::keySubstitute(tFieldSubstitutionMap &pSubs,
                                std::list<tKeyVal> &pParsedArgs,
                                ApplicationScope::eApplicationScope scope,
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
            if (s.mScope & ApplicationScope::BODY)
                keySubOnBody = true;
            if (s.mScope & ApplicationScope::HEADER)
                keySubOnHeader = true;
        }
     }

    bool lDidSubstitute = false;
    // Perform the key substitutions
    if (keySubOnHeader) {
        // On the header
        lDidSubstitute = keySubstitute(pCommands.mSubstitutions,
                                       pHeaderParsedArgs,
                                       ApplicationScope::HEADER,
                                       pRequest.mArgs);
    }
    if (keySubOnBody) {
        // On the body
        std::list<tKeyVal> lParsedArgs;
        parseArgs(lParsedArgs, pRequest.mBody);
        lDidSubstitute |= keySubstitute(pCommands.mSubstitutions,
                                       lParsedArgs,
                                       ApplicationScope::BODY,
                                       pRequest.mBody);
    }
    // Run the raw substitutions
    BOOST_FOREACH(tSubstitute &s, pCommands.mRawSubstitutions) {
        if (s.mScope & ApplicationScope::BODY) {
            pRequest.mBody = boost::regex_replace(pRequest.mBody, s.mRegex, s.mReplacement, boost::match_default | boost::format_all);
        }
        if (s.mScope & ApplicationScope::HEADER) {
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
const tFilter *
RequestProcessor::processRequest(const std::string &pConfPath, RequestInfo &pRequest) {
    std::map<std::string, tRequestProcessorCommands>::iterator it = mCommands.find(pConfPath);

    // No filters for this path
    if (it == mCommands.end() || (!it->second.mFilters.size() && !it->second.mRawFilters.size()))
        return NULL;

    tRequestProcessorCommands lCommands = (*it).second;

    std::list<std::pair<std::string, std::string> > lParsedArgs;
    parseArgs(lParsedArgs, pRequest.mArgs);

    // Tests if at least one active filter matches
    const tFilter* matchedFilter = NULL;
    if (!(matchedFilter = argsMatchFilter(pRequest, lCommands, lParsedArgs))) {
        Log::debug("No args match filter");
        return NULL;
    }

    Log::debug("Filter match");

    // We have a match, perform substitutions
    substituteRequest(pRequest, lCommands, lParsedArgs);
    return matchedFilter;
}

void
RequestProcessor::setUrlCodec(const std::string &pUrlCodec)
{
    mUrlCodec.reset(getUrlCodec(pUrlCodec));
}

void
sendPlainBody(CURL *curl, struct curl_slist *slist, const RequestInfo &rInfo){
    slist = curl_slist_append(slist, "Content-Type: text/xml; charset=utf-8");
    // Avoid Expect: 100 continue
    slist = curl_slist_append(slist, "Expect:");

    std::string contentLen = std::string("Content-Length: ") +
        boost::lexical_cast<std::string>(rInfo.mBody.size());
    slist = curl_slist_append(slist, contentLen.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, rInfo.mBody.size());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, rInfo.mBody.c_str());
}

std::string *
sendDupFormat(CURL *curl, struct curl_slist *slist, const RequestInfo &rInfo, const AnswerHolder &a){
    Log::debug("*** SendDupFormat ***");
    slist = curl_slist_append(slist, "Content-Type: text/xml; charset=utf-8");
    // Avoid Expect: 100 continue
    slist = curl_slist_append(slist, "Expect:");

    //Computing dup format string
    std::stringstream ss;
    //Request body
    ss << std::setfill('0') << std::setw(8) << rInfo.mBody.length() << rInfo.mBody;
    // Answer header
    ss << std::setfill('0') << std::setw(8) << a.m_header.length() << a.m_header;
    // Answer Body
    ss << std::setfill('0') << std::setw(8) << a.m_body.length() << a.m_body;


    std::string *content = new std::string(ss.str());

    std::string contentLen = std::string("Content-Length: ") +
        boost::lexical_cast<std::string>(content->size());
    slist = curl_slist_append(slist, contentLen.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, content->size());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strdup(content->c_str()));
    Log::debug("Content: @@@%s@@@", content->c_str());
    return content;
}

void
RequestProcessor:: performCurlCall(CURL *curl, const tFilter &matchedFilter, const RequestInfo &rInfo) {

    Log::debug("**** PERFORMING CURL CALL");
    AnswerHolder *a = NULL;
    // reply with answer mode check
    if (DuplicationType::value == DuplicationType::REQUEST_WITH_ANSWER) {
        // fetching the request answer object completed
        a = getAnswer(rInfo.mId);
        // Wait for the answer to be fetched
        a->m_sync.lock();
        Log::debug("*****Answer synced: %d", (int)a->m_body.length());
    }

    // Setting URI
    std::string uri = matchedFilter.mDestination + rInfo.mPath + "?" + rInfo.mArgs;
    curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());

    // Sending body in plain or dup format according to the duplication need
    struct curl_slist *slist = NULL;
    std::string *content = NULL;
    if (DuplicationType::value == DuplicationType::REQUEST_WITH_ANSWER) {
        content = sendDupFormat(curl, slist, rInfo, *a);
    } else if (rInfo.hasBody()){
        sendPlainBody(curl, slist, rInfo);
    } else {
        // Regular GET case
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
    }

    Log::debug("Duplicating: %s", uri.c_str());

    int err = curl_easy_perform(curl);
    if (slist)
        curl_slist_free_all(slist);

    if (err == CURLE_OPERATION_TIMEDOUT) {
        __sync_fetch_and_add(&mTimeoutCount, 1);
    } else if (err) {
        Log::error(403, "Sending request failed with curl error code: %d, request:%s", err, uri.c_str());
    }

    delete content;
    if (DuplicationType::value == DuplicationType::REQUEST_WITH_ANSWER) {
        // remove answer from map
        rmAnswer(rInfo.mId);
        //delete a;
    }
}

/**
 * @brief Run the infinite loop which pops new requests of the given queue, processes them and sends the over to the configured destination
 * @param pQueue the queue which gets filled with incoming requests
 */
void
RequestProcessor::run(MultiThreadQueue<RequestInfo> &pQueue)
{
    Log::debug("New worker thread started");

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
        const tFilter *matchedFilter;
        if ((matchedFilter = processRequest(lQueueItem.mConfPath, lQueueItem))) {
            __sync_fetch_and_add(&mDuplicatedCount, 1);
            performCurlCall(lCurl, *matchedFilter, lQueueItem);
            delete matchedFilter;
        }
    }
    curl_easy_cleanup(lCurl);
}

AnswerHolder *
RequestProcessor::getAnswer(unsigned int requestId) {
    AnswerHolder *ret = NULL;
    boost::lock_guard<boost::mutex> guard(mAnswerSync);
    std::map<unsigned int, AnswerHolder *>::iterator a;
    a = mAnswers.find(requestId);
    if (a == mAnswers.end()) {
        ret = new AnswerHolder();
        mAnswers[requestId] = ret;
    }
    else
        ret = a->second;
    return ret;
}

void
RequestProcessor::rmAnswer(unsigned int requestId) {
    boost::lock_guard<boost::mutex> guard(mAnswerSync);
    mAnswers.erase(requestId);
}

tFilterBase::tFilterBase(const std::string &r, ApplicationScope::eApplicationScope s)
    : mScope(s)
    , mRegex(r) {
}

tFilterBase::~tFilterBase() {
}

tFilter::tFilter(const std::string &regex, ApplicationScope::eApplicationScope scope,
                 const std::string &currentDupDestination)
    : tFilterBase(regex, scope)
    , mDestination(currentDupDestination) {
}

    tFilter::tFilter(const tFilter& other): tFilterBase(other) {
    if (&other == this)
        return;
    mField = other.mField;
    mDestination = other.mDestination;
}

tFilterBase::tFilterBase(const tFilterBase &other) {
    if (&other == this)
        return;
    mScope = other.mScope;
    mRegex = other.mRegex;
}

tSubstitute::tSubstitute(const std::string &regex, const std::string &replacement, ApplicationScope::eApplicationScope scope)
    : tFilterBase(regex, scope)
    , mReplacement(replacement){
}

AnswerHolder::AnswerHolder(const std::string &header, const std::string &body)
    : m_header(header)
    , m_body(body)
    , m_sync() {
    m_sync.lock();
}

AnswerHolder::AnswerHolder()
    : m_header()
    , m_body()
    , m_sync() {
    m_sync.lock();
}

AnswerHolder::~AnswerHolder() {
    Log::debug("Destroyed");
}


}
