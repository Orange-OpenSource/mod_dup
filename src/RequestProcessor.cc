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
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>

#include <httpd.h>
#include <iomanip>
#include <set>

using namespace std;

#include "RequestProcessor.hh"
#include "mod_dup.hh"


namespace DupModule {

const char * gUserAgent = "mod-dup";

bool
Commands::toDuplicate() {
    static bool GlobalInit = false;
    static __thread bool lInitialized = false;
    static __thread struct random_data lRD = { 0, 0, 0, 0, 0, 0, 0} ;
    static __thread char lRSB[8];

    if (!GlobalInit) {
        srandom(getpid());
        GlobalInit = true;
    }

    // Initialized per thread
    int lRet = 0;
    if ( ! lInitialized ) {
        memset(lRSB,0, 8);
        // init State must be different for all threads or each will answer the same sequence
        lRet = initstate_r(random(), lRSB, 8, &lRD);
        if (lRet) {
            Log::error(523, "Failed to Initialize Random State");
        }
        lInitialized = true;
    }

    // Thread-safe calls with thread local initialization
    int randNum = 1;
    lRet = random_r(&lRD, &randNum);
    if (lRet) {
        Log::error(524, "random_r failed");
        // No duplication
        return false;
    }

    return ((randNum % 100) < static_cast<int>(mDuplicationPercentage));
}


void
RequestProcessor::setTimeout(const unsigned int &pTimeout) {
    mTimeout = pTimeout;
}

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

const unsigned int
RequestProcessor::getDuplicatedCount() {
    // Atomic read + reset
    // Works because mDuplicatedCount & 0 == 0
    unsigned int lCount = __sync_fetch_and_and(&mDuplicatedCount, 0);
    return lCount;
}

void
RequestProcessor::addFilter(const std::string &pPath, const std::string &pField, const std::string &pFilter,
        const DupConf &pAssociatedConf, tFilter::eFilterTypes fType) {

    mCommands[pPath].mCommands[pAssociatedConf.currentDupDestination].mFilters.insert(std::pair<std::string, tFilter>(boost::to_upper_copy(pField),
            tFilter(pFilter, pAssociatedConf.currentApplicationScope,
                    pAssociatedConf.currentDupDestination, pAssociatedConf.getCurrentDuplicationType(),
            fType)));
}

void
RequestProcessor::setDestinationDuplicationPercentage(const std::string &pPath, const std::string &destination,
                                                      int percentage) {
    mCommands[pPath].mCommands[destination].mDuplicationPercentage = percentage;
}

void
RequestProcessor::addRawFilter(const std::string &pPath, const std::string &pFilter,
        const DupConf &pAssociatedConf, tFilter::eFilterTypes fType) {

    mCommands[pPath].mCommands[pAssociatedConf.currentDupDestination].mRawFilters.push_back(tFilter(pFilter, pAssociatedConf.currentApplicationScope,
            pAssociatedConf.currentDupDestination, pAssociatedConf.getCurrentDuplicationType(),
            fType));
}

void
RequestProcessor::addSubstitution(const std::string &pPath, const std::string &pField, const std::string &pMatch,
        const std::string &pReplace,  const DupConf &pAssociatedConf) {
    mCommands[pPath].mCommands[pAssociatedConf.currentDupDestination].mSubstitutions[boost::to_upper_copy(pField)].push_back(tSubstitute(pMatch, pReplace,
            pAssociatedConf.currentApplicationScope));
}

void
RequestProcessor::addRawSubstitution(const std::string &pPath, const std::string &pRegex, const std::string &pReplace,
        const DupConf &pAssociatedConf){
    mCommands[pPath].mCommands[pAssociatedConf.currentDupDestination].mRawSubstitutions.push_back(tSubstitute(pRegex, pReplace,
            pAssociatedConf.currentApplicationScope));
}

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
RequestProcessor::keyFilterMatch(std::multimap<std::string, tFilter> &pFilters, const std::list<tKeyVal> &pParsedArgs,
        ApplicationScope::eApplicationScope scope, tFilter::eFilterTypes fType){

    BOOST_FOREACH (const tKeyVal &lKeyVal, pParsedArgs) {
        // Key Iteration
        std::pair<std::multimap<std::string, tFilter>::iterator,
        std::multimap<std::string, tFilter>::iterator> lFilterIter = pFilters.equal_range(lKeyVal.first);
        // FilterIteration
        for (std::multimap<std::string, tFilter>::iterator it = lFilterIter.first; it != lFilterIter.second; ++it) {
            if ((it->second.mScope & scope) &&                                  // Scope check
                    it->second.mFilterType == fType) {                              // Filter type check
                if (boost::regex_search(lKeyVal.second, it->second.mRegex)) {
                    return &it->second;
                }
            }
        }
    }
    // No matching
    return NULL;
}

template <class T>
void applicationOn(const T &list, int &header, int &body) {
    header = body = 0;
    BOOST_FOREACH(const typename T::value_type &f, list) {
        if (f.mScope & ApplicationScope::BODY)
            body = true;
        if (f.mScope & ApplicationScope::HEADER)
            header = true;
    }
}

template <class T>
void applicationOnMap(const T &list, int &header, int &body) {
    header = body = 0;
    BOOST_FOREACH(const typename T::value_type &f, list) {
        if (f.second.mScope & ApplicationScope::BODY)
            body = true;
        if (f.second.mScope & ApplicationScope::HEADER)
            header = true;
    }
}

const tFilter *
RequestProcessor::argsMatchFilter(RequestInfo &pRequest, Commands &pCommands, std::list<tKeyVal> &pHeaderParsedArgs) {

    const tFilter *matched = NULL;
    std::multimap<std::string, tFilter> &pFilters = pCommands.mFilters;

    // Key filter type deection
    int keyFilterOnHeader, keyFilterOnBody;
    applicationOnMap(pFilters, keyFilterOnHeader, keyFilterOnBody);

    Log::debug("Filters on body: %d | on header: %d", keyFilterOnBody, keyFilterOnHeader);

    // Prevent Filtering check on HEADER
    if (keyFilterOnHeader && (matched = keyFilterMatch(pFilters, pHeaderParsedArgs, ApplicationScope::HEADER, tFilter::PREVENT_DUPLICATION))) {
        Log::debug("PREVENT Filter on HEADER match");
        return NULL;
    }

    std::list<tKeyVal> lParsedArgs;

    // Prevent Filtering check on BODY
    if (keyFilterOnBody){
        parseArgs(lParsedArgs, pRequest.mBody);
        if ((matched = keyFilterMatch(pFilters, lParsedArgs, ApplicationScope::BODY, tFilter::PREVENT_DUPLICATION))) {
            Log::debug("PREVENT Filter on BODY match");
            return NULL;
        }
    }

    // Raw filters prevent analyse
    BOOST_FOREACH (tFilter &raw, pCommands.mRawFilters) {
        if (raw.mFilterType == tFilter::PREVENT_DUPLICATION) {
            // Header application
            if (raw.mScope & ApplicationScope::HEADER) {
                if (boost::regex_search(pRequest.mArgs, raw.mRegex)) {
                    Log::debug("Prevent Raw filter (HEADER) matched: %s | %s", pRequest.mArgs.c_str(), raw.mRegex.str().c_str());
                    return NULL;
                }
            }
            // Body application
            if (raw.mScope & ApplicationScope::BODY) {
                if (boost::regex_search(pRequest.mBody, raw.mRegex)) {
                    Log::debug("Prevent Raw filter (BODY) matched: %s | %s", pRequest.mBody.c_str(), raw.mRegex.str().c_str());
                    return NULL;
                }
            }
        }
    }

    // Key filters on header
    if (keyFilterOnHeader && (matched = keyFilterMatch(pFilters, pHeaderParsedArgs, ApplicationScope::HEADER, tFilter::REGULAR))){
        Log::debug("Filter on HEADER match");
        return matched;
    }

    // Key filters on body
    if (keyFilterOnBody){
        if ((matched = keyFilterMatch(pFilters, lParsedArgs, ApplicationScope::BODY, tFilter::REGULAR))) {
            Log::debug("Filter on BODY match");
            return matched;
        }
    }

    // Raw filters matching
    BOOST_FOREACH (tFilter &raw, pCommands.mRawFilters) {
        if (raw.mFilterType != tFilter::PREVENT_DUPLICATION) {
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
RequestProcessor::substituteRequest(RequestInfo &pRequest, Commands &pCommands, std::list<tKeyVal> &pHeaderParsedArgs) {
    // Ideally we would use the pool from the apache request, but it's used in another thread

    bool keySubOnBody, keySubOnHeader;
    keySubOnBody = keySubOnHeader = false;
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
    BOOST_FOREACH(const tSubstitute &s, pCommands.mRawSubstitutions) {
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

std::list<const tFilter *>
RequestProcessor::processRequest(RequestInfo &pRequest, std::list<std::pair<std::string, std::string> > parsedArgs) {
    std::list<const tFilter *> ret;

    const std::string &pConfPath = pRequest.mConfPath;
    std::map<std::string, CommandsByDestination>::iterator it = mCommands.find(pConfPath);

    // No settings for this path or no duplication mechanism
    if (it == mCommands.end())
        return ret;

    CommandsByDestination &lCommands = (*it).second;

    // For each duplication destination
    std::map<std::string, Commands>::iterator itb = lCommands.mCommands.begin(),
        itbe = lCommands.mCommands.end();
    while (itb != itbe) {
        Log::debug("### Duplication tested: %s", itb->first.c_str() );
        // Tests if at least one active filter matches on this duplication location
        const tFilter* matchedFilter = NULL;
        if ((matchedFilter = argsMatchFilter(pRequest, itb->second, parsedArgs))) {
            ret.push_back(matchedFilter);
        }
        ++itb;
    }
    return ret;
}

RequestProcessor::RequestProcessor() :
            mTimeout(0), mTimeoutCount(0),
            mDuplicatedCount(0) {
    setUrlCodec();
}

void
RequestProcessor::setUrlCodec(const std::string &pUrlCodec)
{
    mUrlCodec.reset(getUrlCodec(pUrlCodec));
}

/// @brief send a POST with a body
/// @param toSend must be kept until the request is performed
void
RequestProcessor::sendInBody(CURL *curl, const RequestInfo &rInfo, curl_slist *&slist, const std::string &toSend) const {
    std::string contentLen = std::string("Content-Length: ") +
            boost::lexical_cast<std::string>(toSend.size());
    slist = curl_slist_append(slist, contentLen.c_str());


    curl_easy_setopt(curl, CURLOPT_POST, 1);
    addOrigHeaders(rInfo, slist);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, toSend.size());
    // the string is not copied by curl, so must be kept until request is performed
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, toSend.c_str());
}

std::string *
RequestProcessor::sendDupFormat(CURL *curl, const RequestInfo &rInfo, curl_slist *&slist) const {
  
    // set the content type to application/x-dup-serialized if we pass the REQUEST_WITH_ANSWER
    slist = curl_slist_append(slist, "Content-Type: application/x-dup-serialized");
    // Adding HTTP HEADER to indicate that the request is duplicated with it's answer
    slist = curl_slist_append(slist, "Duplication-Type: Response");
    //Computing dup format string
    std::stringstream ss;
    //Request body
    RequestInfo::Serialize(rInfo.mBody, ss);

    // Answer headers, Copy requestInfo out headers
    std::string answerHeaders;
    BOOST_FOREACH(const RequestInfo::tHeaders::value_type &v, rInfo.mHeadersOut) {
        answerHeaders.append(v.first + std::string(": ") + v.second + "\n");
    }
    RequestInfo::Serialize(answerHeaders, ss);

    // Answer Body
    RequestInfo::Serialize(rInfo.mAnswer, ss);
    std::string *content = new std::string(ss.str());
    sendInBody(curl, rInfo, slist, *content);
    return content;
}

/// @brief add the original input headers making sure we have no duplicates
/// duplicates are merged into csv by apache
/// @param rInfo
/// @param slist
void RequestProcessor::addOrigHeaders(const RequestInfo &rInfo, struct curl_slist *&slist) {
    // Copy the request input headers
  
    // Create a set of headers already added
    std::set<std::string> headers;
    
    curl_slist * curlist = slist;
    while ( curlist ) {
      if ( curlist->data ) {
	  char *pos = strchr(curlist->data, ':');
	  if ( pos ) {
	      std::string header;
	      header.assign(curlist->data, pos - curlist->data);
	      headers.insert(header);
	  }
      }
      curlist = curlist->next;
    }

    /* The headers potentially added by dup or others and not to be added twice are currently the following:
      ELAPSED_TIME_BY_DUP,X_DUP_HTTP_STATUS,X_DUP_METHOD,X_DUP_CONTENT_TYPE,
      Duplication-Type,Content-Length,Host,Expect,Transfer-Encoding,Content-Type
    */
    
    // Now append only if not in the set of headers already added
    // or apache will at some point concatenate values in a csv list
    // but also never add Transfer-Encoding chunked or a Content-Length, or Duplication-Type
    // because we may not be adding it but a previous duplication might have put it there
    BOOST_FOREACH(const RequestInfo::tHeaders::value_type &v, rInfo.mHeadersIn) {
        if ( (headers.find(v.first) == headers.end()) && (v.first != std::string("Host")) && 
	  (v.first != std::string("Transfer-Encoding")) && 
	  (v.first != std::string("Content-Length")) && (v.first != std::string("Duplication-Type")) ) {
            headers.insert(v.first);
            slist = curl_slist_append(slist, std::string(v.first + std::string(": ") + v.second).c_str());
            Log::error(11, "Adding header %s: %s", v.first.c_str(), v.second.c_str());
      } else {
            Log::error(11, "Skipping copy of header %s", v.first.c_str());
        }
    }
}

/// @brief add http headers common to all dup types
/// @param slist slist ref on which to add
void RequestProcessor::addCommonHeaders(const RequestInfo &rInfo, struct curl_slist *&slist) {
    // Add the elapsed time header
    std::string elapsed = std::string("ELAPSED_TIME_BY_DUP: ") + boost::lexical_cast<std::string>(rInfo.getElapsedTimeMS());
    slist = curl_slist_append(slist, elapsed.c_str());

    // This avoids the Expect: 100 continue
    // Which is generated by curl when it's a POST and the body is long
    slist = curl_slist_append(slist, "Expect:");
    // Setting X-DUPLICATED-REQUEST to 1 in the header
    slist = curl_slist_append(slist, "X-DUPLICATED-REQUEST: 1");
    // Setting mod-dup as the real agent for tracability
    slist = curl_slist_append(slist, "User-RealAgent: mod-dup");
}

void
RequestProcessor::performCurlCall(CURL *curl, const tFilter &matchedFilter, const RequestInfo &rInfo) {
    // Setting URI
    std::string uri = matchedFilter.mDestination + rInfo.mPath + "?" + rInfo.mArgs;
    curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());

    std::string *content = NULL;
    struct curl_slist *slist = NULL;
    
    addCommonHeaders(rInfo, slist);

    // Sending body in plain or dup format according to the duplication need
    if (matchedFilter.mDuplicationType == DuplicationType::REQUEST_WITH_ANSWER) {
        // POST with dup serialized original request body AND response
        content = sendDupFormat(curl, rInfo, slist);
    } else if ((matchedFilter.mDuplicationType == DuplicationType::COMPLETE_REQUEST) && rInfo.hasBody()) {
        // POST with original body
        sendInBody(curl, rInfo, slist, rInfo.mBody);
    } else {
        // Regular GET case
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
        addOrigHeaders(rInfo, slist);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    }

    Log::debug(">> Duplicating: %s", uri.c_str());

    int err = curl_easy_perform(curl);
    if (slist)
        curl_slist_free_all(slist);

    if (err == CURLE_OPERATION_TIMEDOUT) {
        __sync_fetch_and_add(&mTimeoutCount, 1);
    } else if (err) {
        Log::error(403, "Sending request failed with curl error code: %d, request:%s", err, uri.c_str());
    }
    delete content;
}

void
RequestProcessor::runOne(RequestInfo &reqInfo, CURL * pCurl) {

    std::list<std::pair<std::string, std::string> > lParsedArgs;
    parseArgs(lParsedArgs, reqInfo.mArgs);


    std::list<const tFilter *> matchedFilters = processRequest(reqInfo, lParsedArgs);
    std::list<const tFilter *>::const_iterator it = matchedFilters.begin(),
        ite = matchedFilters.end();
        while (it != ite) {

            // First get a hand the commands structure that matches the destination duplication
            CommandsByDestination &cbd = mCommands.at(reqInfo.mConfPath);
            Commands &c = cbd.mCommands.at((*it)->mDestination);

            // Should we drop the duplication?
            if (!c.toDuplicate()) {
                Log::debug("Regulation drop");
                ++it;
                continue;
            }

            if (!c.mSubstitutions.empty() || !c.mRawSubstitutions.empty()) {
                // perform substitutions specific to this location
                RequestInfo b(reqInfo);
                substituteRequest(b, c, lParsedArgs);
                performCurlCall(pCurl, **it, b);

            } else {
                performCurlCall(pCurl, **it, reqInfo);
            }

            __sync_fetch_and_add(&mDuplicatedCount, 1);
            ++it;
    }
}

CURL * RequestProcessor::initCurl()
{
    CURL * lCurl = curl_easy_init();
    if (!lCurl) {
        Log::error(402, "Could not init curl request object.");
        return NULL;
    }
    curl_easy_setopt(lCurl, CURLOPT_USERAGENT, gUserAgent);
    // Activer l'option provoque des timeouts sur des requests avec un fort payload
    curl_easy_setopt(lCurl, CURLOPT_TIMEOUT_MS, mTimeout);
    curl_easy_setopt(lCurl, CURLOPT_NOSIGNAL, 1);

    return lCurl;
}

/**
 * @brief Run the infinite loop which pops new requests of the given queue, processes them and sends the over to the configured destination
 * @param pQueue the queue which gets filled with incoming requests
 */
void
RequestProcessor::run(MultiThreadQueue<boost::shared_ptr<RequestInfo> > &pQueue)
{
    Log::debug("New worker thread started");

    CURL * lCurl = initCurl();
    if (!lCurl) {
        return;
    }

    for (;;) {
        boost::shared_ptr<RequestInfo> lQueueItemShared = pQueue.pop();
        RequestInfo *lQueueItem = lQueueItemShared.get();

        if (lQueueItem->isPoison()) {
            // Master tells us to stop
            Log::debug("Received poison pill. Exiting.");
            break;
        }
        runOne(*lQueueItem, lCurl);
    }
    curl_easy_cleanup(lCurl);
}

tElementBase::tElementBase(const std::string &r, ApplicationScope::eApplicationScope s)
: mScope(s)
, mRegex(r) {
}

tElementBase::tElementBase(const std::string &regex,
        boost::regex::flag_type flags,
        ApplicationScope::eApplicationScope scope)
: mScope(scope)
, mRegex(regex, flags) {

}

tElementBase::~tElementBase() {
}

tFilter::tFilter(const std::string &regex, ApplicationScope::eApplicationScope scope,
        const std::string &currentDupDestination,
        DuplicationType::eDuplicationType dupType,
        tFilter::eFilterTypes fType)
: tElementBase(regex, scope)
, mDestination(currentDupDestination)
, mDuplicationType(dupType)
, mFilterType(fType) {
}

tFilter::~tFilter() {
}

tElementBase::tElementBase(const tElementBase &other) {
    if (&other == this)
        return;
    mScope = other.mScope;
    mRegex = other.mRegex;
}

tSubstitute::tSubstitute(const std::string &regex, const std::string &replacement, ApplicationScope::eApplicationScope scope)
: tElementBase(regex, scope)
, mReplacement(replacement){
}


tSubstitute::~tSubstitute() {
}


}
