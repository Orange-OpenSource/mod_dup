/*
 * multilineDiffPrinter.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "multilineDiffPrinter.hh"

namespace LibWsDiff {

multilineDiffPrinter::~multilineDiffPrinter() {}

void multilineDiffPrinter::addInfo(std::string& key, std::string& value){
	this->diffString << key << " : " << value << std::endl;
}

void multilineDiffPrinter::addRequestUri(std::string& uri){
	this->diffString << uri << std::endl;
}

void multilineDiffPrinter::addRequestHeader(std::string& key,std::string& value){
	this->headerReqString << key << " : " << value << std::endl;
}
void multilineDiffPrinter::addStatus(std::string& service, int statusCode){
	this->statusString << " "<< service << " " << statusCode;
}

void multilineDiffPrinter::addHeaderDiff(std::string& key,std::string& srcValue,std::string& dstValue)
{
	this->headerDiffString << key << " ==> " << srcValue << "/" << dstValue << std::endl;
}

void multilineDiffPrinter::addFullDiff(std::vector<std::string> diffLines){
	for(std::vector<std::string>::iterator it=diffLines.begin();it!=diffLines.end();++it){
		this->bodyDiffString << *it << std::endl;
	}
}

bool multilineDiffPrinter::retrieveDiff(std::string& res){
	std::string separator(19,'-');

	this->diffString << separator << this->headerDiffString
			<< separator << this->bodyDiffString;
	this->diffString << "END DIFFERENCE : " << this->id;
	res=this->diffString.str();
	return this->isADiff;
}

} /* namespace LibWsDiff */
