/*
 * multilineDiffPrinter.hh
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#ifndef MULTILINEDIFFPRINTER_HH_
#define MULTILINEDIFFPRINTER_HH_

#include<sstream>

#include "diffPrinter.hh"

namespace LibWsDiff {

class multilineDiffPrinter: public LibWsDiff::diffPrinter {
private:
	std::stringstream diffString;
	std::stringstream headerReqString;
	std::stringstream headerDiffString;
	std::stringstream bodyDiffString;
	std::stringstream statusString;

public:
	multilineDiffPrinter(std::string id);
	virtual ~multilineDiffPrinter();


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
#endif /* MULTILINEDIFFPRINTER_HH_ */
