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
		const diffTypeAvailable type){
	switch (type) {
		case diffTypeAvailable::MULTILINE:
			return new multilineDiffPrinter(id);
			break;
		default:
			//Default Case is the json case
			return new jsonDiffPrinter(id);
			break;
	}
}

}; /* namespace LibWsDiff */
