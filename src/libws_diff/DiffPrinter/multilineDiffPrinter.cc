/*
 * multilineDiffPrinter.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "multilineDiffPrinter.hh"
#include <boost/assign.hpp>

namespace LibWsDiff {

const std::string multilineDiffPrinter::SEPARATOR="-------------------\n";

const std::map<multilineDiffPrinter::diffPartitionning,std::string> multilineDiffPrinter::prettyfyStartLine=
		boost::assign::map_list_of
		(RUNTIME,"Elapsed time for requests (ms):")
		(URI,"URI : ")
		(STATUS,"Http Status Codes:");

const std::map<multilineDiffPrinter::diffPartitionning,std::string> multilineDiffPrinter::prettyfyEndLine=
		boost::assign::map_list_of
		(RUNTIME,"\n")
		(STATUS,"\n");

std::string* multilineDiffPrinter::getStream(const diffPartitionning part){
	stringmap::iterator it=this->streams.find(part);
	if(it==this->streams.end()){
		this->streams.insert(std::make_pair(part,new std::string()));
		it=this->streams.find(part);
	}
	return it->second;
}

multilineDiffPrinter::multilineDiffPrinter(std::string id):diffPrinter(id),hadPreviousCassDiff(false){
	this->getStream(diffPartitionning::BEGIN)->append("BEGIN NEW REQUEST DIFFERENCE n: "+this->id+"\n");
}

multilineDiffPrinter::~multilineDiffPrinter() {}

void multilineDiffPrinter::addInfo(const std::string& key,const std::string& value){
	this->getStream(diffPartitionning::INFO)->append(key+" : " +value +"\n");
}

void multilineDiffPrinter::addInfo(const std::string& key,const double value){
	this->getStream(diffPartitionning::INFO)->append(key+" : " +boost::lexical_cast<std::string>(value) +"\n");
}

void multilineDiffPrinter::addRequestUri(const std::string& uri,const std::string& paramsBody){
	this->getStream(diffPartitionning::URI)->append(uri);
	if(!paramsBody.empty()){
		this->getStream(diffPartitionning::URI)->append(paramsBody);
	}
	this->getStream(diffPartitionning::URI)->append("\n");
}

void multilineDiffPrinter::addRequestHeader(const std::string& key,const std::string& value){
	this->getStream(diffPartitionning::HEADER)->append(key+" : "+value+"\n");
}
void multilineDiffPrinter::addStatus(const std::string& service,const int statusCode){
	this->getStream(diffPartitionning::STATUS)->append(" "+service+" "+boost::lexical_cast<std::string>(statusCode));
}

void multilineDiffPrinter::addRuntime(const std::string& service,const int milli){
	this->getStream(diffPartitionning::RUNTIME)->append(" "+service+" "+boost::lexical_cast<std::string>(milli));
}

void multilineDiffPrinter::addHeaderDiff(const std::string& key,
		const boost::optional<std::string> srcValue,
		const boost::optional<std::string> dstValue)
{
	this->isADiff=true;
	this->getStream(diffPartitionning::HEADERDIFF)->append(key+" ==> "+srcValue.get_value_or("")+"/"+dstValue.get_value_or("")+"\n");
}

void multilineDiffPrinter::addCassandraDiff(const std::string& fieldName,
				const std::string& multiValue,
				const std::string& dbValue,
				const std::string& reqValue){
	this->isADiff=true;

	this->getStream(diffPartitionning::CASSDIFF)->append(
			(this->hadPreviousCassDiff ? SEPARATOR : "" )+
			"Field name in the db : '"+fieldName+"'\n"
			"Multivalue/Collection index/key : '" + multiValue + "'\n"
			"Value retrieved in Database : '" + dbValue + "'\n"
			"Value about to be set from Request : '" + reqValue + "'\n");
	this->hadPreviousCassDiff=true;
}

void multilineDiffPrinter::addFullDiff(std::vector<std::string> diffLines,
		const int truncSize,
		const std::string& type){
	this->isADiff=true;
	for(std::vector<std::string>::iterator it=diffLines.begin();it!=diffLines.end();++it){
		this->getStream(diffPartitionning::BODYDIFF)->append(*it+"\n");
	}
}

void multilineDiffPrinter::addFullDiff(std::string& diffLines,
		const int truncSize,
		const std::string& type){
	this->isADiff=true;
	this->getStream(diffPartitionning::BODYDIFF)->append(diffLines+"\n");
}

bool multilineDiffPrinter::retrieveDiff(std::string& res){
	if(this-isADiff){
		for(stringmap::iterator it=this->streams.begin();it!=this->streams.end();it++){
			if(!it->second->empty()){
				//Adding header for the current part
				if(prettyfyStartLine.find(it->first)!=prettyfyStartLine.end()){
					res.append(prettyfyStartLine.at(it->first));
				}

				res.append(*(it->second));

				//Adding pretty end of line for the current part
				if(prettyfyEndLine.find(it->first)!=prettyfyEndLine.end()){
					res.append(prettyfyEndLine.at(it->first));
				}
				res.append(SEPARATOR);
			}
		}
		res.append("END DIFFERENCE : "+ this->id + "\n");
	}
	return this->isADiff;
}

} /* namespace LibWsDiff */
