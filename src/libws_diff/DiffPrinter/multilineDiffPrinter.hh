/*
 * multilineDiffPrinter.hh
 *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#ifndef MULTILINEDIFFPRINTER_HH_
#define MULTILINEDIFFPRINTER_HH_

#include<map>

#include "diffPrinter.hh"
#include<boost/lexical_cast.hpp>

namespace LibWsDiff {

/**
 * Concrete class implementing a multiline human readable format of the diff
 * Example :
BEGIN NEW REQUEST DIFFERENCE n°: 123 / Elapsed time for diff computation : 1ms
Elapsed time for requests (ms): DUP 432 COMP 0 DIFF 432



ELAPSED_TIME_BY_DUP: 432
agent-type: myAgent
content-type: plain/text
date: TODAY

MyClientRequest
-------------------
Http Status Codes: DUP 456 COMP 654
-------------------
myHeaderDiff
-------------------
myBodyDiff
END DIFFERENCE n°:123
 *
 */
class multilineDiffPrinter: public LibWsDiff::diffPrinter {

private:

	static const std::string SEPARATOR;
	bool hadPreviousCassDiff; //Allow us to not print a separator on the first cass diff
	enum diffPartitionning{
			BEGIN,
			INFO,
			RUNTIME,
			STATUS,
			URI,
			HEADER,
            BODY,
			CASSDIFF,
			HEADERDIFF,
			BODYDIFF
		};

	static const std::map<diffPartitionning,std::string> prettyfyStartLine;
	static const std::map<diffPartitionning,std::string> prettyfyEndLine;

	typedef std::map<diffPartitionning,std::string*> stringmap;

	stringmap streams;

	std::string* getStream(const diffPartitionning part);

public:
	multilineDiffPrinter(std::string id);
	virtual ~multilineDiffPrinter();

	virtual void addInfo(const std::string& key,const std::string& value);
	virtual void addInfo(const std::string& key,const double value);
	virtual void addRequestUri(const std::string& uri,const std::string& paramsBody="");
    virtual void addRequestBody(const std::string& body);
    virtual void addRequestHeader(const std::string& key,const std::string& value);
	virtual void addStatus(const std::string& service,const int statusCode);
	virtual void addRuntime(const std::string& service,const int milli);

	virtual void addHeaderDiff(const std::string& key,
			const boost::optional<std::string> srcValue,
			const boost::optional<std::string> dstValue);

	virtual void addCassandraDiff(const std::string& fieldName,
					const std::string& multiValue,
					const std::string& dbValue,
					const std::string& reqValue);

	virtual void addFullDiff(std::vector<std::string> diffLines,
							const int truncSize=100,
							const std::string& type="XML");

	virtual void addFullDiff(std::string& diffLines,
							const int truncSize=100,
							const std::string& type="XML");

	virtual bool retrieveDiff(std::string& res);
    
    static std::string partitionSeparator(diffPartitionning d);

};

} /* namespace LibWsDiff */
#endif /* MULTILINEDIFFPRINTER_HH_ */
