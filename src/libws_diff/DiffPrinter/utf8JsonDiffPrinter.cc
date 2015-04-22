/*
 * jsonDiffPrinter.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#include "utf8JsonDiffPrinter.hh"
#include "utf8.h"
#include<string>
#include<exception>

namespace LibWsDiff {

utf8JsonDiffPrinter::utf8JsonDiffPrinter(std::string id):jsonDiffPrinter(id){}

utf8JsonDiffPrinter::~utf8JsonDiffPrinter() {}

bool utf8JsonDiffPrinter::retrieveDiff(std::string& res){
	bool v;
	v=jsonDiffPrinter::retrieveDiff(res);
	try{
	    	std::string temp;
	    	utf8::replace_invalid(res.begin(), res.end(), back_inserter(temp));
	    	res = temp;
	}catch (...) {
		return false;
	}
	return v;
}

} /* namespace LibWsDiff */
