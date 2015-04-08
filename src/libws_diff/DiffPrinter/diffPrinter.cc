/*
 * diffPrinter.cc
 *
 *  Created on: Apr 8, 2015
 *      Author: cvallee
 */

#include "diffPrinter.hh"
#include "jsonDiffPrinter.hh"
#include "multilineDiffPrinter.hh"

namespace LibWsDiff {

diffPrinter* diffPrinter::createDiffPrinter(const std::string& id,
		const std::string& type){
	if(std::strcmp("json",type.c_str())){
		return new jsonDiffPrinter(id);
	}else{
		return new multilineDiffPrinter(id);
	}
}

}; /* namespace LibWsDiff */
