/*
 * jsonDiffPrinter.hh
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#ifndef JSONDIFFPRINTER_HH_
#define JSONDIFFPRINTER_HH_

#define JSONMAXSIZE 14000

#include<sstream>
#include <jsoncpp/json/json.h>

#include <boost/algorithm/string.hpp>
#include<boost/lexical_cast.hpp>

#include "diffPrinter.hh"


namespace LibWsDiff {
/**
 * Concrete class implementing a json formating of the diff logs
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
class jsonDiffPrinter: public LibWsDiff::diffPrinter {
private:
	Json::Value jsonRes;
public:
	jsonDiffPrinter(std::string id);
	virtual ~jsonDiffPrinter();

	virtual void addInfo(const std::string& key,const std::string& value);
	virtual void addInfo(const std::string& key,const double value);
	virtual void addRequestUri(const std::string& uri,
			const std::string& paramsBody=std::string());
	virtual void addRequestHeader(const std::string& key,const std::string& value);
	virtual void addStatus(const std::string& service,const int statusCode);
	virtual void addRuntime(const std::string& service,const int milli);

	virtual void addCassandraDiff(const std::string& fieldName,
				const std::string& multiValue,
				const std::string& dbValue,
				const std::string& reqValue);

	virtual void addHeaderDiff(const std::string& key,
			const boost::optional<std::string> srcValue,
			const boost::optional<std::string> dstValue);

	virtual void addFullDiff(std::vector<std::string> diffLines,
							const int truncSize=100,
							const std::string& type="XML");

	virtual void addFullDiff(std::string& diffLines,
				const int truncSize=100,
				const std::string& type="XML");

	virtual bool retrieveDiff(std::string& res);
};

} /* namespace LibWsDiff */
#endif /* JSONDIFFPRINTER_HH_ */
