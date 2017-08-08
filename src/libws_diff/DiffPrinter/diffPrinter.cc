/*
 * diffPrinter.cc
 *
 *  Created on: Apr 8, 2015
 *      Author: cvallee
 */

#include "diffPrinter.hh"
#include "jsonDiffPrinter.hh"
#include "multilineDiffPrinter.hh"
#include "utf8JsonDiffPrinter.hh"

namespace LibWsDiff {

diffPrinter* diffPrinter::createDiffPrinter(const std::string& id,
		const diffTypeAvailable type){
	switch (type) {
		case diffTypeAvailable::MULTILINE:
			return new multilineDiffPrinter(id);
		case diffTypeAvailable::JSON:
			return new jsonDiffPrinter(id);
		default:
			//Default Case is the json case
			return new utf8JsonDiffPrinter(id);
	}
}

std::string diffPrinter::diffTypeStr(diffPrinter::diffTypeAvailable d)
{
    switch (d) {
        case diffTypeAvailable::MULTILINE:
            return "MULTILINE";
        case diffTypeAvailable::JSON:
            return "SIMPLEJSON";
        default:
            return "UTF8JSON";
    }
}


}; /* namespace LibWsDiff */
