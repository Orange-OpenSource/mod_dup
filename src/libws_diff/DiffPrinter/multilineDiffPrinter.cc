/*
 * multilineDiffPrinter.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "multilineDiffPrinter.hh"

namespace LibWsDiff {

std::string multilineDiffPrinter::getStream(const std::string& part){
	stringmap::iterator it=this->streams.find(part);
	if(it==this->streams.end()){
		this->streams.insert(std::make_pair(part,new std::string()));
		it=this->streams.find(part);
	}
	return *(it->second);
}

multilineDiffPrinter::multilineDiffPrinter(std::string id):diffPrinter(id){
	this->getStream("main").append("BEGIN NEW REQUEST DIFFERENCE n: "+this->id+"\n");
}

multilineDiffPrinter::~multilineDiffPrinter() {}

void multilineDiffPrinter::addInfo(const std::string& key,const std::string& value){
	this->getStream("main").append(key+" : " +value +"\n");
}

void multilineDiffPrinter::addInfo(const std::string& key,const double value){
	this->getStream("main").append(key+" : " +boost::lexical_cast<std::string>(value) +"\n");
}

void multilineDiffPrinter::addRequestUri(const std::string& uri,const std::string& paramsBody){
	this->getStream("main").append(uri);
	if(!paramsBody.empty()){
		this->getStream("main").append(paramsBody);
	}
	this->getStream("main").append("\n");
}

void multilineDiffPrinter::addRequestHeader(const std::string& key,const std::string& value){
	this->getStream("main").append(key+" : "+value+"\n");
}
void multilineDiffPrinter::addStatus(const std::string& service,const int statusCode){
	this->getStream("status").append(" "+service+" "+boost::lexical_cast<std::string>(statusCode));
}

void multilineDiffPrinter::addRuntime(const std::string& service,const int milli){
	this->getStream("runTime").append(" "+service+" "+boost::lexical_cast<std::string>(milli));
}

void multilineDiffPrinter::addHeaderDiff(const std::string& key,
		const boost::optional<std::string> srcValue,
		const boost::optional<std::string> dstValue)
{
	this->isADiff=true;
	this->getStream("headerDiff").append(key+" ==> "+srcValue.get()+"/"+dstValue.get()+"\n");
}

void multilineDiffPrinter::addCassandraDiff(const std::string& fieldName,
				const std::string& multiValue,
				const std::string& dbValue,
				const std::string& reqValue){
	this->isADiff=true;
	this->getStream("cass").append("Field name in the db : '"+fieldName+"'\n"
			"Multivalue/Collection index/key : '" + multiValue + "'\n"
			"Value retrieved in Database : '" + dbValue + "'\n"
			"Value about to be set from Request : '" + reqValue + "'\n");
}

void multilineDiffPrinter::addFullDiff(std::vector<std::string> diffLines,
		const int truncSize,
		const std::string& type){
	this->isADiff=true;
	for(std::vector<std::string>::iterator it=diffLines.begin();it!=diffLines.end();++it){
		this->getStream("body").append(*it+"\n");
	}
}

void multilineDiffPrinter::addFullDiff(std::string& diffLines,
		const int truncSize=100,
		const std::string& type="XML"){
	this->isADiff=true;
	this->getStream("body").append(diffLines);
}

bool multilineDiffPrinter::retrieveDiff(std::string& res){
	std::string separator(19,'-');
	separator.append("\n");
	for(stringmap::iterator it=this->streams.begin();it!=this->streams.end();it++){
		res.append(*(it->second)+separator);
	}
	res.append("END DIFFERENCE : "+ this->id + "\n");
	return this->isADiff;
}

} /* namespace LibWsDiff */
