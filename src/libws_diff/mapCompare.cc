/*
 * mapCompare.cc
 *
 *  Created on: 11 fev. 2014
 *      Author: cvallee
 */


#include "mapCompare.hh"
#include <sstream>
#include <vector>
#include <boost/regex.hpp>
#include <boost/bind.hpp>

namespace LibWsDiff {

MapCompare::MapCompare(){}

MapCompare::~MapCompare(){}

void MapCompare::addIgnoreRegex(const std::string& key,const std::string& myregex){
	mIgnoreRegex.insert(std::pair<std::string,boost::regex>(key,boost::regex(myregex)));
}

void MapCompare::addStopRegex(const std::string& key,const std::string& myregex){
	mStopRegex.insert(std::pair<std::string,boost::regex>(key,boost::regex(myregex)));
}

bool MapCompare::checkStop(const mapStrings& map) const{
	for(mapStrings::const_iterator it = map.begin();it!=map.end();++it){
		mapKeyRegex::const_iterator itStop = mStopRegex.find(it->first);
		if ( itStop != mStopRegex.end() &&  boost::regex_search(it->second,itStop->second)) {
			return true;
		}
	}
	return false;
}

void MapCompare::applyIgnoreRegex(mapStrings& map) const{
	std::vector<mapStrings::iterator> toDelete;
	for(mapStrings::iterator it = map.begin();it!=map.end();++it){
		mapKeyRegex::const_iterator itIgnore = mIgnoreRegex.find(it->first);
		if (itIgnore != mIgnoreRegex.end()){
			it->second=boost::regex_replace(it->second,itIgnore->second,"");
			if(it->second == ""){
				toDelete.push_back(mapStrings::iterator(it));
			}
		}
	}
	for(std::vector<mapStrings::iterator>::iterator it=toDelete.begin();it!=toDelete.end();++it){
		map.erase(*it);
	}
}

bool MapCompare::retrieveDiff(const mapStrings& src,const mapStrings& dst,std::string& output) const{
	std::map<std::string,std::string> diffSrc,diffDst;
	std::map<std::string,std::pair<std::string,std::string> > valueDiff;
	std::ostringstream stream;

	if(checkStop(src) || checkStop(dst)){
		return false;
	}

	mapStrings dupSrc(src),dupDst(dst);
	applyIgnoreRegex(dupSrc);
	applyIgnoreRegex(dupDst);

	//Current problem is that differences match either key and value differences
	std::set_difference(dupSrc.begin(),dupSrc.end(),dupDst.begin(),dupDst.end(),std::inserter(diffSrc,diffSrc.begin()));
	std::set_difference(dupDst.begin(),dupDst.end(),dupSrc.begin(),dupSrc.end(),std::inserter(diffDst,diffDst.begin()));
	if (diffSrc.size()>0){
		stream << "Key missing in the destination map :" << std::endl;
		for(std::map<std::string,std::string>::iterator it=diffSrc.begin();it!=diffSrc.end();++it){
			std::map<std::string,std::string>::iterator itDst=diffDst.find(it->first);
			if (itDst==  diffDst.end()){
				stream << "\'" << it->first<< "\' ==> " << "\'" << it->second << "\'" << std::endl;
			}else{
				valueDiff.insert(std::pair<std::string,std::pair<std::string,std::string> >(it->first,std::pair<std::string,std::string>(it->second,itDst->second)));
				diffDst.erase(itDst);
			}
		}
	}
	if (diffDst.size()>0){
		stream << "Key missing in src map :" << std::endl;
		for(std::map<std::string,std::string>::iterator it=diffDst.begin();it!=diffDst.end();++it){
			stream << "\'" << it->first<< "\' ==> " << "\'" << it->second << "\'" << std::endl;
		}
	}
	if (valueDiff.size()>0){
		stream << "Key with value differences :" << std::endl;
		for(std::map<std::string,std::pair<std::string,std::string> >::iterator it=valueDiff.begin();it!=valueDiff.end();++it){
			stream << "\'" << it->first<< "\' ==> "
					<< "\'" << it->second.first << "\'/\'"
					<< it->second.second << "\'" << std::endl;
		}
	}
	output=stream.str();
	return true;
}

bool MapCompare::retrieveDiff(const mapStrings& src,const mapStrings& dst,LibWsDiff::diffPrinter* printer) const{
	std::map<std::string,std::string> diffSrc,diffDst;
	std::map<std::string,std::pair<std::string,std::string> > valueDiff;

	if(checkStop(src) || checkStop(dst)){
		return false;
	}

	mapStrings dupSrc(src),dupDst(dst);
	applyIgnoreRegex(dupSrc);
	applyIgnoreRegex(dupDst);

	//Current problem is that differences match either key and value differences
	std::set_difference(dupSrc.begin(),dupSrc.end(),dupDst.begin(),dupDst.end(),std::inserter(diffSrc,diffSrc.begin()));
	std::set_difference(dupDst.begin(),dupDst.end(),dupSrc.begin(),dupSrc.end(),std::inserter(diffDst,diffDst.begin()));
	if (diffSrc.size()>0){
		for(std::map<std::string,std::string>::iterator it=diffSrc.begin();it!=diffSrc.end();++it){
			std::map<std::string,std::string>::iterator itDst=diffDst.find(it->first);
			if (itDst==  diffDst.end()){
				printer->addHeaderDiff(std::string(it->first),std::string(it->second),NULL);
			}else{
				valueDiff.insert(std::pair<std::string,std::pair<std::string,std::string> >(it->first,std::pair<std::string,std::string>(it->second,itDst->second)));
				diffDst.erase(itDst);
			}
		}
	}
	if (diffDst.size()>0){
		for(std::map<std::string,std::string>::iterator it=diffDst.begin();it!=diffDst.end();++it){
			printer->addHeaderDiff(std::string(it->first),NULL,std::string(it->second));
		}
	}
	if (valueDiff.size()>0){
		for(std::map<std::string,std::pair<std::string,std::string> >::iterator it=valueDiff.begin();it!=valueDiff.end();++it){
			printer->addHeaderDiff(std::string(it->first),std::string(it->second.first),std::string(it->second.second));
		}
	}
	return true;
}

};  /* namespace LibWsDiff */

