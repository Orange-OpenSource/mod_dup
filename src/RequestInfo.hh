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

#include <boost/shared_ptr.hpp>
#include <list>
#include <map>
#include <string>
#include <sstream>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/basic_text_oarchive.hpp>

struct apr_bucket_brigade;
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
        /** @brief The location (in the conf) which matched this query. */
        std::string mConfPath;
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

        typedef std::list<std::pair<std::string, std::string> > tHeaders;

        /** @brief list that represents the headers of the incoming request */
        tHeaders mHeadersIn;

        /** @brief list that represents the headers of the request answer */
        tHeaders mHeadersOut;

        /**
         * @brief Constructs the object using the three strings.
         * @param pConfPath The location (in the conf) which matched this query
         * @param pPath The path part of the request
         * @param pConfPath The parameters part of the query (without leading ?)
         */
        RequestInfo(std::string id, const std::string &pConfPath, const std::string &pPath,
                    const std::string &pArgs, const std::string *body = 0);

        /**
         * @brief Constructor dedicated for serialization purpose
         */
        RequestInfo(mapStr reqHeader,std::string reqBody,mapStr respHeader,std::string respBody,mapStr dupHeader,std::string dupBody);

        /**
         * @brief Constructs a request initialising it's id
         */
        RequestInfo(std::string id);

        /**
         * @brief Constructs a poisonous object causing the processor to stop when read
         */
        RequestInfo();

        /**
         * returns true if the request has a body
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

    };
}
