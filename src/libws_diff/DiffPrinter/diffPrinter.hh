/*
 * diffPrinter.hh
 *
 * Abstract Class defining the behaviour of log_compare
  *
 *  Created on: Apr 1, 2015
 *      Author: cvallee
 */

#ifndef DIFFPRINTER_HH_
#define DIFFPRINTER_HH_

#include <vector>
#include <string>
#include <boost/optional.hpp>
#include <boost/algorithm/string.hpp>

namespace LibWsDiff {


/**
 * Abstract interface wrapping the diff printing format
 */
class diffPrinter {

protected:
	bool isADiff; //Store the presence of diff elements
	std::string id; //Request id
public:

	enum diffTypeAvailable {
		JSON,
		MULTILINE
	};

	diffPrinter(std::string id):isADiff(false),id(id){}
	virtual ~diffPrinter(){}

	bool isDiff(){return this->isADiff;}

	/**
	 * Basic factory-like constructor method
	 */
	static diffPrinter* createDiffPrinter(const std::string& id,
			const diffTypeAvailable type=diffTypeAvailable::JSON);

	/***
	 * Add simple key value information
	 */
	virtual void addInfo(const std::string& key,const std::string& value)=0;

	/***
	 * Add simple key value information
	 */
	virtual void addInfo(const std::string& key,const double value)=0;

	/***
	 * Add the URI information
	 */
	virtual void addRequestUri(const std::string& uri,const std::string& paramsBody="")=0;

	/***
	 * Add the Header of the request information
	 */
	virtual void addRequestHeader(const std::string& key,const std::string& value)=0;

	/***
	 * Add a status information about a service
	 */
	virtual void addStatus(const std::string& service,const int statusCode)=0;

	/***
	 * Add a status information about a service
	 */
	virtual void addRuntime(const std::string& service,const int milli)=0;

	/***
	 * Add a status information about a service
	 */
	virtual void addCassandraDiff(const std::string& fieldName,
			const std::string& multiValue,
			const std::string& dbValue,
			const std::string& reqValue)=0;

	/***
	 * Add a information of diff between a source and a destination for a
	 * specific key
	 */
	virtual void addHeaderDiff(const std::string& key,
			const boost::optional<std::string> srcValue,
			const boost::optional<std::string> dstValue)=0;

	/***
	 * Add the diff information concerning the full body of the request
	 * \param[in] vector of string, 1 line per line of body
	 */
	virtual void addFullDiff(std::vector<std::string> diffLines,
			const int truncSize=100,
			const std::string& type="XML")=0;

	virtual void addFullDiff(std::string& diffLines,
			const int truncSize=100,
			const std::string& type="XML")=0;

	/***
	 * Output the diff in the specified format
	 * \param[in] : the string into which we'll append the diff
	 * \param[out] : bool : True if one diff at least (body or header)
	 */
	virtual bool retrieveDiff(std::string& res)=0;
};

} /* namespace LibWsDiff */
#endif /* DIFFPRINTER_HH_ */
