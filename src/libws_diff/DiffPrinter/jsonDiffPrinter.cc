/*
 * jsonDiffPrinter.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "jsonDiffPrinter.hh"
#include<string>
#include<regex>

namespace LibWsDiff {

jsonDiffPrinter::jsonDiffPrinter(std::string id):diffPrinter(id){
	this->jsonRes["id"]=id;
}

jsonDiffPrinter::~jsonDiffPrinter() {
	// TODO Auto-generated destructor stub
}

void jsonDiffPrinter::addInfo(const std::string& key,const std::string& value){
	this->jsonRes[key]=value;
}

void jsonDiffPrinter::addInfo(const std::string& key,const double value){
	this->jsonRes[key]=value;
}

void jsonDiffPrinter::addRequestUri(const std::string& uri,
		const std::string& paramsBody){
	this->jsonRes["request"]["uri"]=uri;
	int pivot=uri.find('?');
	this->jsonRes["request"]["url"]=uri.substr(0,pivot);
	std::vector<std::string> vectParams;
	if(pivot!=uri.length()){
		std::string params=uri.substr(pivot+1,uri.length());

		boost::split(vectParams,params,boost::is_any_of("&"));
		for(std::vector<std::string>::iterator it=vectParams.begin();
				it!=vectParams.end();++it){
			int pos=it->find('=');
			this->jsonRes["request"]["args"][it->substr(0,pos)]=it->substr(pos+1,it->length());
		}
	}
	vectParams.clear();
	if(!paramsBody.empty()){
		boost::split(vectParams,paramsBody,boost::is_any_of("&"));
		for(std::vector<std::string>::iterator it=vectParams.begin();
				it!=vectParams.end();++it){
			int pos=it->find('=');
			this->jsonRes["request"]["args"][it->substr(0,pos)]=it->substr(pos+1,it->length());
		}
	}
}

void jsonDiffPrinter::addRequestHeader(const std::string& key,const std::string& value){
	this->jsonRes["request"]["header"][key]=value;
}

void jsonDiffPrinter::addStatus(const std::string& service,const int statusCode){
	this->jsonRes["status"][service]=statusCode;
}

void jsonDiffPrinter::addRuntime(const std::string& service,const int milli){
	this->jsonRes["runtime"][service]=milli;
}

void jsonDiffPrinter::addCassandraDiff(const std::string& fieldName,
				const std::string& multiValue,
				const std::string& dbValue,
				const std::string& reqValue){
	this->isADiff=true;
	std::string field=fieldName;
	int i=1;
	while(!this->jsonRes.isMember(field)){
		field=fieldName + boost::lexical_cast<std::string>(i++);
	}
	this->jsonRes["diff"]["cassandra"][field]["mulVal"]=multiValue;
	this->jsonRes["diff"]["cassandra"][field]["dbVal"]=dbValue;
	this->jsonRes["diff"]["cassandra"][field]["reqVal"]=reqValue;
}

void jsonDiffPrinter::addHeaderDiff(const std::string& key,
		const boost::optional<std::string> srcValue,
		const boost::optional<std::string> dstValue){
	this->isADiff=true;
	if(srcValue){
		this->jsonRes["diff"]["header"][key]["src"]=srcValue.get();
	}
	if(dstValue){
		this->jsonRes["diff"]["header"][key]["dst"]=dstValue.get();
	}
}

void jsonDiffPrinter::addFullDiff(std::vector<std::string> diffLines,
		const int truncSize,
		const std::string& type){

	//TODO identify following ids
	if(type=="XML"){

	}
	int posDiff=0,negDiff=0;
	std::set<std::string> negList,posList;

	std::string fullDiff= boost::algorithm::join(diffLines,"\n");

	//Analyze and retrieve counter and list of diff
	std::regex xmlFirstTag("(<.*?>.*)");
	std::smatch base_match;
	for(std::vector<std::string>::iterator it=diffLines.begin();it!=diffLines.end();++it){
		if(std::regex_match(*it,base_match,xmlFirstTag)){
			if(it->find('+') < it->find('<')){
				posDiff+=1;
				if(base_match.size()>=1){
					posList.insert(base_match[0].str());
				}
			}else if(it->find('-') < it->find('<')){
				negDiff+=1;
				if(base_match.size()>=1){
					negList.insert(base_match[0].str());
				}
			}
		}
	}

	if(fullDiff.length()>=JSONMAXSIZE){
		this->isADiff=true;
		std::string truncatedDiff;
		//Let's retrieve only the first 100 char of each line
		for(std::vector<std::string>::iterator it=diffLines.begin();it!=diffLines.end();++it){
			truncatedDiff+=it->substr(0,truncSize);
			truncatedDiff+="\n";
		}
		//If still too big then don't include
		if (truncatedDiff.length()<JSONMAXSIZE){
			this->jsonRes["diff"]["body"]["full"]=truncatedDiff;
		}
	}else{
		if(!fullDiff.empty()){
			this->isADiff=true;
			this->jsonRes["diff"]["body"]["full"]=fullDiff;
		}
	}
}

void jsonDiffPrinter::addFullDiff(std::string& diffLines,
		const int truncSize,
		const std::string& type){
	std::vector<std::string> split;
	boost::split(split,diffLines,boost::is_any_of("\n"),boost::token_compress_on);
	this->addFullDiff(split,truncSize,type);
}

bool jsonDiffPrinter::retrieveDiff(std::string& res){
	//TODO Check size not too big once again else what to do?
	std::stringstream result;
	Json::FastWriter oneLineJson;
	result << oneLineJson.write(this->jsonRes);
	res.append(result.str());
	return this->isADiff;
}

} /* namespace LibWsDiff */
