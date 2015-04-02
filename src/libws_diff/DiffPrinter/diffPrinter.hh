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

namespace LibWsDiff {

class diffPrinter {
protected :
	bool isADiff; //Store the presence of diff elements
	int id;

public:
	diffPrinter(int id):isADiff(false),id(id){}
	virtual ~diffPrinter(){}

	/***
	 * Add simple key value information
	 */
	virtual void addInfo(const std::string& key,const std::string& value)=0;

	/***
	 * Add the URI information
	 */
	virtual void addRequestUri(const std::string& uri)=0;

	/***
	 * Add the Header of the request information
	 */
	virtual void addRequestHeader(const std::string& key,const std::string& value)=0;

	/***
	 * Add a status information about a service
	 */
	virtual void addStatus(const std::string& service,const int statusCode)=0;

	/***
	 * Add a information of diff between a source and a destination for a
	 * specific key
	 */
	virtual void addHeaderDiff(const std::string& key,
			const std::string& srcValue,
			const std::string& dstValue)=0;

	/***
	 * Add the diff information concerning the full body of the request
	 * \param[in] vector of string, 1 line per line of body
	 */
	virtual void addFullDiff(std::vector<std::string> diffLines,
			const int truncSize,
			const std::string& type)=0;

	/***
	 * Output the diff in the specified format
	 * \param[in] : the string into which we'll append the diff
	 * \param[out] : bool : True if one diff at least (body or header)
	 */
	virtual bool retrieveDiff(std::string& res)=0;
};

} /* namespace LibWsDiff */
#endif /* DIFFPRINTER_HH_ */
