/*
 * utf8JsonDiffPrinter.hh
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#pragma once

#include "jsonDiffPrinter.hh"

namespace LibWsDiff {
/**
 * Concrete class implementing a json formating of the diff logs
 * The format is the same as its mother but check utf8
 * WARNING : One line format json
 *
 * Example :
	'{"id":"2","diff":{"header":{"myHeaderKey":"{"src":"srcValue","dst":"dstValue"}}}
,"request":{"uri":"/spp/main?version=1.0.0&country=FR&sid=ADVNEC&request=getPNS
&infos=AdviseCapping","url":"/spp/main","args":{"version":"1.0.0","country":"FR"
,"sid":"ADVNEC","request":"getPNS","infos":"AdviseCapping"},"header":{"X-ACCEPT
ED":"Hello"}},"status":{"DUP":400,"COMP":200}}'
 *
 */
class utf8JsonDiffPrinter: public LibWsDiff::jsonDiffPrinter {
public:
	utf8JsonDiffPrinter(std::string id);
	virtual ~utf8JsonDiffPrinter();

	virtual bool retrieveDiff(std::string& res);
};

} /* namespace LibWsDiff */
