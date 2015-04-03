/*
 * jsonDiffPrinter.hh
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#ifndef JSONDIFFPRINTER_HH_
#define JSONDIFFPRINTER_HH_

#define JSONMAXSIZE 14000

#include<sstream>
#include <jsoncpp/json/json.h>

#include <boost/algorithm/string.hpp>

#include "diffPrinter.hh"


namespace LibWsDiff {

class jsonDiffPrinter: public LibWsDiff::diffPrinter {
private:
	Json::Value jsonRes;
public:
	jsonDiffPrinter(std::string id);
	virtual ~jsonDiffPrinter();

	virtual void addInfo(const std::string& key,const std::string& value);
	virtual void addRequestUri(const std::string& uri);
	virtual void addRequestHeader(const std::string& key,const std::string& value);
	virtual void addStatus(const std::string& service,const int statusCode);

	virtual void addHeaderDiff(const std::string& key,
			const boost::optional<std::string> srcValue,
			const boost::optional<std::string> dstValue);

	virtual void addFullDiff(std::vector<std::string> diffLines,
							const int truncSize,
							const std::string& type);

	virtual bool retrieveDiff(std::string& res);
};

} /* namespace LibWsDiff */
#endif /* JSONDIFFPRINTER_HH_ */
