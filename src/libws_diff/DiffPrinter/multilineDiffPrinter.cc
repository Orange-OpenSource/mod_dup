/*
 * multilineDiffPrinter.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "multilineDiffPrinter.hh"

namespace LibWsDiff {

multilineDiffPrinter::multilineDiffPrinter(std::string id):diffPrinter(id){
	this->diffString << "BEGIN NEW REQUEST DIFFERENCE n: "<<this->id << std::endl;
}

multilineDiffPrinter::~multilineDiffPrinter() {}

void multilineDiffPrinter::addInfo(const std::string& key,const std::string& value){
	this->diffString << key << " : " << value << std::endl;
}

void multilineDiffPrinter::addRequestUri(const std::string& uri){
	this->diffString << uri << std::endl;
}

void multilineDiffPrinter::addRequestHeader(const std::string& key,const std::string& value){
	this->headerReqString << key << " : " << value << std::endl;
}
void multilineDiffPrinter::addStatus(const std::string& service,const int statusCode){
	this->statusString << " "<< service << " " << statusCode;
}

void multilineDiffPrinter::addHeaderDiff(const std::string& key,
		const boost::optional<std::string> srcValue,
		const boost::optional<std::string> dstValue)
{
	this->headerDiffString << key << " ==> " << srcValue << "/" << dstValue << std::endl;
}

void multilineDiffPrinter::addFullDiff(std::vector<std::string> diffLines,
		const int truncSize,
		const std::string& type){
	for(std::vector<std::string>::iterator it=diffLines.begin();it!=diffLines.end();++it){
		this->bodyDiffString << *it << std::endl;
	}
}

bool multilineDiffPrinter::retrieveDiff(std::string& res){
	std::string separator(19,'-');
	separator.append("\n");

	this->diffString << separator << this->headerDiffString
			<< separator << this->bodyDiffString;
	this->diffString << "END DIFFERENCE : " << this->id << std::endl;
	res=this->diffString.str();
	return this->isADiff;
}

} /* namespace LibWsDiff */
