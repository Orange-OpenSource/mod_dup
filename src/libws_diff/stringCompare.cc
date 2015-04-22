/*
 * StringCompare.cpp
 *
 *  Created on: 23 janv. 2014
 *      Author: cvallee
 */

#include "stringCompare.hh"
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include "dtl.hpp"
#include "variables.hpp"
#include "functors.hpp"
#include "customPrinter.hpp"

namespace LibWsDiff {

StringCompare::StringCompare(){}

StringCompare::~StringCompare() {
	// TODO Auto-generated destructor stub
}

StringCompare::StringCompare(const tStrings& stopRegex,const tStrings& ignoreRegex){
	for(tStrings::const_iterator it=stopRegex.begin();it!=stopRegex.end();++it){
		addStopRegex(*it);
	}
	for(tStrings::const_iterator it=ignoreRegex.begin();it!=ignoreRegex.end();++it){
		addIgnoreRegex(*it);
	}
}

void StringCompare::ignoreCases(std::string & str) const{
	for(tRegexes::const_iterator it=mIgnoreRegex.begin();it!=mIgnoreRegex.end();++it){
		str = boost::regex_replace(str,*it,"");
	}
}

bool StringCompare::checkStopRegex(const std::string& str) const{
	for(tRegexes::const_iterator it=mStopRegex.begin();it!=mStopRegex.end();++it){
		if (boost::regex_search(str,*it)){
			return true;
		}
	}
	return false;
}

bool StringCompare::vectDiff(const tStrings& src,const tStrings& dst, std::string& output) const{
	dtl::Diff<std::string,tStrings > d(src,dst);
	d.onUnserious();
	d.compose();
	d.composeUnifiedHunks();
	//retrieve UnifiedHunks under a vector format rework it and process it into ignoreCases.

	std::vector<dtl::uniHunk<std::pair<std::string, dtl::eleminfo> >, std::allocator<dtl::uniHunk<std::pair<std::string, dtl::eleminfo> > > > diff= d.getUniHunks();
	std::ostringstream stream;
	std::for_each(diff.begin(),diff.end(),dtl::UniHunkPrinter<std::pair<std::string, dtl::eleminfo> >(stream));

	//return the diff representation
	output = stream.str();
	return true;
}

void StringCompare::addIgnoreRegex(const std::string& re){
	mIgnoreRegex.push_back(boost::regex(re));
}

void StringCompare::addStopRegex(const std::string& re){
	mStopRegex.push_back(boost::regex(re));
}

bool StringCompare::retrieveDiff(const std::string & src,const std::string& dst, std::string& output) const{
	//Check if stopCase in orig string
	if (checkStopRegex(src) || checkStopRegex(dst)){
		return false;
	}

	std::string in(src),out(dst);
	ignoreCases(in);
	ignoreCases(out);

	dtl::Diff<char,std::string> d(in,out);
	d.onUnserious();
	d.compose();
	d.composeUnifiedHunks();
	//retrieve UnifiedHunks under a vector format rework it

	std::vector<dtl::uniHunk<std::pair<char, dtl::eleminfo> >, std::allocator<dtl::uniHunk<std::pair<char, dtl::eleminfo> > > > diff= d.getUniHunks();
	std::ostringstream stream;
	std::for_each(diff.begin(),diff.end(),dtl::customHunkPrinter<std::pair<char, dtl::eleminfo> >(stream));

	//return the diff representation
	output=stream.str();
	return true;
}

bool StringCompareHeader::retrieveDiff(const std::string& src,const std::string& dst,std::string& output) const{
	tStrings linesSrc,linesDst;
	if (checkStopRegex(src) || checkStopRegex(dst)){
			return false;
	}
	std::string in(src),out(dst);
	ignoreCases(in);
	ignoreCases(out);

	boost::split(linesSrc,in,boost::is_any_of("\n"),boost::token_compress_on);

	boost::split(linesDst,out,boost::is_any_of("\n"),boost::token_compress_on);
	return vectDiff(linesSrc,linesDst,output);
}

bool StringCompareHeader::retrieveDiff(const std::string& src,const std::string& dst,LibWsDiff::diffPrinter& printer) const{
	std::string out;
	bool res = this->retrieveDiff(src,dst,out);
	printer.addFullDiff(out);
	return res;
}

bool StringCompareBody::retrieveDiff(const std::string& src,const std::string& dst,std::string& output) const{
	tStrings linesSrc,linesDst;
	if (checkStopRegex(src) || checkStopRegex(dst)){
			return false;
	}
	std::string in(src),out(dst);
	ignoreCases(in);
	ignoreCases(out);

	boost::replace_all(in,"><",">\n<");
	boost::split(linesSrc,in,boost::is_any_of("\n"),boost::token_compress_on);

	boost::replace_all(out,"><",">\n<");
	boost::split(linesDst,out,boost::is_any_of("\n"),boost::token_compress_on);
	return vectDiff(linesSrc,linesDst,output);
}

bool StringCompareBody::retrieveDiff(const tStrings& src,const tStrings& dst,std::string& output) const{
	//Check if stopCase in orig string
	tStrings srcCopy(src);
	tStrings dstCopy(dst);
	for(tStrings::iterator it = srcCopy.begin();it!=srcCopy.end();++it){
		if (checkStopRegex(*it)){
			return false;
		}else{
			ignoreCases(*it);
		}
	}
	for(tStrings::iterator it = dstCopy.begin();it!=dstCopy.end();++it){
		if (checkStopRegex(*it)){
			return false;
		}else{
			ignoreCases(*it);
		}
	}
	return vectDiff(srcCopy,dstCopy,output);
}

bool StringCompareBody::retrieveDiff(const tStrings& src,
		const tStrings& dst,
		LibWsDiff::diffPrinter& printer) const{
	std::string out;
	bool res = this->retrieveDiff(src,dst,out);
	printer.addFullDiff(out);
	return res;
}

bool StringCompareBody::retrieveDiff(const std::string& src,
		const std::string& dst,
		LibWsDiff::diffPrinter& printer) const{
	std::string out;
	bool res = this->retrieveDiff(src,dst,out);
	printer.addFullDiff(out);
	return res;
}

} /* namespace LibWsDiff */
