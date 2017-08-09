/*
 * jsonDiffPrinter.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "jsonDiffPrinter.hh"
#include<string>
#include<set>
#include<boost/regex.hpp>

namespace LibWsDiff {

jsonDiffPrinter::jsonDiffPrinter(std::string id): diffPrinter(id)
{
    mJsonRes["id"] = id;
    printerType = diffTypeAvailable::JSON;
}

jsonDiffPrinter::~jsonDiffPrinter()
{
    // TODO Auto-generated destructor stub
}

void jsonDiffPrinter::addInfo(const std::string& key, const std::string& value)
{
    mJsonRes[key] = value;
}

void jsonDiffPrinter::addInfo(const std::string& key, const double value)
{
    mJsonRes[key] = value;
}

void jsonDiffPrinter::addRequestBody(const std::string& body)
{
    addInfo("ReqBody",body);
}

void jsonDiffPrinter::addRequestUri(const std::string& uri,
                                    const std::string& paramsBody)
{
    mJsonRes["request"]["uri"] = uri;
    size_t pivot = uri.find('?');
    mJsonRes["request"]["url"] = uri.substr(0, pivot);
    std::vector<std::string> vectParams;
    if(pivot != uri.length()) {
        std::string params = uri.substr(pivot + 1, uri.length());

        boost::split(vectParams, params, boost::is_any_of("&"));
        for(std::vector<std::string>::iterator it = vectParams.begin();
                it != vectParams.end(); ++it) {
            int pos = it->find('=');
            mJsonRes["request"]["args"][boost::to_upper_copy(it->substr(0, pos))] = it->substr(pos + 1, it->length());
        }
    }
    vectParams.clear();
    if(!paramsBody.empty()) {
        boost::split(vectParams, paramsBody, boost::is_any_of("&"));
        for(std::vector<std::string>::iterator it = vectParams.begin();
                it != vectParams.end(); ++it) {
            int pos = it->find('=');
            mJsonRes["request"]["args"][boost::to_upper_copy(it->substr(0, pos))] = it->substr(pos + 1, it->length());
        }
    }
}

void jsonDiffPrinter::addRequestHeader(const std::string& key, const std::string& value)
{
    mJsonRes["request"]["header"][boost::to_upper_copy(key)] = value;
}

void jsonDiffPrinter::addStatus(const std::string& service, const int statusCode)
{
    mJsonRes["status"][service] = statusCode;
}

void jsonDiffPrinter::addRuntime(const std::string& service, const int milli)
{
    mJsonRes["runtime"][service] = milli;
}

void jsonDiffPrinter::addCassandraDiff(const std::string& fieldName,
                                       const std::string& multiValue,
                                       const std::string& dbValue,
                                       const std::string& reqValue)
{
    isADiff = true;
    std::string lField = fieldName;
    int i = 1;
    while(mJsonRes["diff"]["cassandra"].isMember(lField)) {
        lField = fieldName + std::to_string(i++);
    }
    mJsonRes["diff"]["cassandra"][lField]["mulVal"] = multiValue;
    mJsonRes["diff"]["cassandra"][lField]["dbVal"] = dbValue;
    mJsonRes["diff"]["cassandra"][lField]["reqVal"] = reqValue;
}

void jsonDiffPrinter::addHeaderDiff(const std::string& key,
                                    const boost::optional<std::string> srcValue,
                                    const boost::optional<std::string> dstValue)
{
    isADiff = true;
    if(srcValue) {
        mJsonRes["diff"]["header"][key]["src"] = srcValue.get();
    }
    if(dstValue) {
        mJsonRes["diff"]["header"][key]["dst"] = dstValue.get();
    }
}

void jsonDiffPrinter::addFullDiff(std::vector<std::string> diffLines,
                                  const int truncSize,
                                  const std::string& type)
{

    //TODO identify following ids

    int posDiff = 0, negDiff = 0;
    std::set<std::string> negList;
    std::set<std::string> posList;

    //Analyze and retrieve counter and list of diff
    boost::regex tagToIdentify;
    std::string firstChar;
    if(type == "XML") {
        tagToIdentify = boost::regex("<.*?>");
        firstChar = '<';
    }
    else {
        tagToIdentify = boost::regex("\".*?\"");
        firstChar = '"';
    }
    boost::smatch what;
    for(std::vector<std::string>::iterator it = diffLines.begin(); it != diffLines.end(); ++it) {
        if(boost::regex_search(*it, what, tagToIdentify)) {
            if((*(it)).find('+') < (*(it)).find(firstChar)) {
                posDiff += 1;
                if(what.size() >= 1) {
                    posList.insert(what[0]);
                }
            }
            else if((*(it)).find('-') < (*(it)).find(firstChar)) {
                negDiff += 1;
                if(what.size() >= 1) {
                    negList.insert(what[0]);
                }
            }
        }
    }
    if(posDiff) {
        mJsonRes["diff"]["body"]["posDiff"] = posDiff;
        Json::Value l(Json::arrayValue);
        for(std::set<std::string>::iterator it = posList.begin(); it != posList.end(); it++)
            l.append(*it);
        mJsonRes["diff"]["body"]["posList"] = l;
    }
    if(negDiff) {
        mJsonRes["diff"]["body"]["negDiff"] = negDiff;
        Json::Value l(Json::arrayValue);
        for(std::set<std::string>::iterator it = negList.begin(); it != negList.end(); it++)
            l.append(*it);
        mJsonRes["diff"]["body"]["negList"] = l;
    }

    std::string fullDiff = boost::algorithm::join(diffLines, "\n");

    if(fullDiff.length() >= JSONMAXSIZE) {
        isADiff = true;
        std::string truncatedDiff;
        //Let's retrieve only the first 100 char of each line
        for(std::vector<std::string>::iterator it = diffLines.begin(); it != diffLines.end(); ++it) {
            truncatedDiff += it->substr(0, truncSize);
            truncatedDiff += "\n";
        }
        //If still too big then don't include
        if (truncatedDiff.length() < JSONMAXSIZE) {
            mJsonRes["diff"]["body"]["full"] = truncatedDiff;
        }
    }
    else {
        if(!fullDiff.empty()) {
            isADiff = true;
            mJsonRes["diff"]["body"]["full"] = fullDiff;
        }
    }
}

void jsonDiffPrinter::addFullDiff(std::string& diffLines,
                                  const int truncSize,
                                  const std::string& type)
{
    std::vector<std::string> split;
    boost::split(split, diffLines, boost::is_any_of("\n"), boost::token_compress_on);
    addFullDiff(split, truncSize, type);
}

bool jsonDiffPrinter::retrieveDiff(std::string& res)
{
    //TODO Check size not too big once again else what to do?
    std::stringstream result;
    Json::FastWriter oneLineJson;
    result << oneLineJson.write(mJsonRes);
    res.append(result.str());
    return isADiff;
}

} /* namespace LibWsDiff */
