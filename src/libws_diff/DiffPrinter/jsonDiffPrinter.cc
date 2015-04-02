/*
 * jsonDiffPrinter.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "jsonDiffPrinter.hh"

namespace LibWsDiff {

jsonDiffPrinter::jsonDiffPrinter(int id):diffPrinter(id){
	this->jsonRes["id"]=id;
}

jsonDiffPrinter::~jsonDiffPrinter() {
	// TODO Auto-generated destructor stub
}

void jsonDiffPrinter::addInfo(const std::string& key,const std::string& value){
	this->jsonRes[key]=value;
}

void jsonDiffPrinter::addRequestUri(const std::string& uri){
	this->jsonRes["request"]["uri"]=uri;
	int pivot=uri.find('?');
	this->jsonRes["request"]["url"]=uri.substr(0,pivot);
	std::string params=uri.substr(pivot+1,uri.length());

	std::vector<std::string> vectParams;
	boost::split(vectParams,params,boost::is_any_of("&"));
	for(std::vector<std::string>::iterator it=vectParams.begin();
			it!=vectParams.end();++it){
		int pos=it->find('=');
		this->jsonRes["request"]["args"][it->substr(0,pos)]=it->substr(pos+1,it->length());
	}

}
void jsonDiffPrinter::addRequestHeader(const std::string& key,const std::string& value){
	this->jsonRes["request"]["header"][key]=value;
}

void jsonDiffPrinter::addStatus(const std::string& service,const int statusCode){
	this->jsonRes["status"][service]=statusCode;
}

void jsonDiffPrinter::addHeaderDiff(const std::string& key,
		const std::string& srcValue,
		const std::string& dstValue){
	this->isADiff=true;
	this->jsonRes["diff"]["header"][key]["src"]=srcValue;
	this->jsonRes["diff"]["header"][key]["dst"]=dstValue;
}

void jsonDiffPrinter::addFullDiff(std::vector<std::string> diffLines,
		const std::string& type=std::string("XML")){
	this->isADiff=true;
	//TODO identify following ids
	this->jsonRes["diff"]["body"]["posDiff"]=0;
	this->jsonRes["diff"]["body"]["negDiff"]=0;
	this->jsonRes["diff"]["body"]["posList"]=0;
	this->jsonRes["diff"]["body"]["negList"]=0;

	std::string fullDiff= boost::algorithm::join(diffLines,"\n");

	if(fullDiff.length()>=JSONMAXSIZE){
		std::string truncatedDiff;
		//Let's retrieve only the first 100 char of each line
		for(std::vector<std::string>::iterator it=diffLines.begin();it!=diffLines.end();++it){
			truncatedDiff+=it->substr(0,100);
			truncatedDiff+="\n";
		}
		//If still too big then don't include
		if (truncatedDiff.length()<JSONMAXSIZE){
			this->jsonRes["diff"]["body"]["full"]=truncatedDiff;
		}
	}else{
		this->jsonRes["diff"]["body"]["full"]=fullDiff;
	}
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
