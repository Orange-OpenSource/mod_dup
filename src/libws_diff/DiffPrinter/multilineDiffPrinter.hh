/*
 * multilineDiffPrinter.hh
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#ifndef MULTILINEDIFFPRINTER_HH_
#define MULTILINEDIFFPRINTER_HH_

#include<streambuf>

#include "diffPrinter.hh"


namespace LibWsDiff {

class multilineDiffPrinter: protected LibWsDiff::diffPrinter {
private:
	std::stringstream diffString;
	std::stringstream headerReqString;
	std::stringstream headerDiffString;
	std::stringstream bodyDiffString;
	std::stringstream statusString;

public:
	virtual ~multilineDiffPrinter();

	virtual void addInfo(std::string& key, std::string& value);
	virtual void addRequestUri(std::string& uri);
	virtual void addRequestHeader(std::string& key,std::string& value);
	virtual void addStatus(std::string& service, int statusCode);

	virtual void addHeaderDiff(std::string& key,std::string& srcValue,std::string& dstValue);
	virtual void addFullDiff(std::vector<std::string> diffLines);

	virtual bool retrieveDiff(std::string& res);

};

} /* namespace LibWsDiff */
#endif /* MULTILINEDIFFPRINTER_HH_ */
