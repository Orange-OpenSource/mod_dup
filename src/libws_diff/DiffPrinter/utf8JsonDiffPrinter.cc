/*
 * jsonDiffPrinter.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "utf8JsonDiffPrinter.hh"
#include "utf8.h"
#include<string>

namespace LibWsDiff {

utf8JsonDiffPrinter::utf8JsonDiffPrinter(std::string id):jsonDiffPrinter(id){}

utf8JsonDiffPrinter::~utf8JsonDiffPrinter() {}

bool utf8JsonDiffPrinter::checkBeforeInsert(std::string& str){
    try{
    	std::string temp;
    	utf8::replace_invalid(str.begin(), str.end(), back_inserter(temp));
    	str = temp;
    }catch (Exception e) {
		return false;
	}
    return true;
}

void utf8JsonDiffPrinter::addInfo(const std::string& key,const std::string& value){
	std::string v1(key);
	std::string v2(value);
	jsonDiffPrinter::addInfo(key,value);

}
void utf8JsonDiffPrinter::addInfo(const std::string& key,const double value);
void utf8JsonDiffPrinter::addRequestUri(const std::string& uri,
		const std::string& paramsBody=std::string());
void utf8JsonDiffPrinter::addRequestHeader(const std::string& key,const std::string& value);
void utf8JsonDiffPrinter::addStatus(const std::string& service,const int statusCode);
void utf8JsonDiffPrinter::addRuntime(const std::string& service,const int milli);

void utf8JsonDiffPrinter::addCassandraDiff(const std::string& fieldName,
			const std::string& multiValue,
			const std::string& dbValue,
			const std::string& reqValue);

void utf8JsonDiffPrinter::addHeaderDiff(const std::string& key,
		const boost::optional<std::string> srcValue,
		const boost::optional<std::string> dstValue);

void utf8JsonDiffPrinter::addFullDiff(std::vector<std::string> diffLines,
						const int truncSize=100,
						const std::string& type="XML");

void utf8JsonDiffPrinter::addFullDiff(std::string& diffLines,
			const int truncSize=100,
			const std::string& type="XML");

bool utf8JsonDiffPrinter::retrieveDiff(std::string& res);

} /* namespace LibWsDiff */
