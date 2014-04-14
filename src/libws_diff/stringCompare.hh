/*
 * StringCompare.h
 *
 *  Created on: 23 janv. 2014
 *      Author: cvallee
 */

#pragma once

#include <vector>
#include <boost/regex.hpp>


typedef std::vector<boost::regex> tRegexes;
typedef std::vector<std::string> tStrings;

namespace LibWsDiff {

/**
 * BasicCompare class
 * Interface providing tools for string comparaison
 */
class StringCompare {
	//Vector of regex to match in order to reject any comparaison
	tRegexes mStopRegex;
	//Vector of regex to remove from any comparaison
	tRegexes mIgnoreRegex;

protected:
	/**Check the str against the initials stop regex provided
	* @param str : the string to validate
	* @return True if any regex is match in the string else false
	*/
	bool checkStopRegex(const std::string & str) const;

	/**
	 * Remove the ignore regex content in the input string
	 * @param str : the input string
	 */
	void ignoreCases(std::string & str) const;

	/**
	 * function factoring the diff between vector of string
	 * @param src : source of the diff
	 * @param dst : destination of the diff
	 * @param output : resulting diff in string format
	 */
	bool vectDiff(const tStrings& src,const tStrings& dst, std::string& output) const;
public:
	/**
	 * Initializes the regexs through vector of string
	 */
	StringCompare(const tStrings& stopRegex,const tStrings& ignoreRegex);
	/**
	 * Default Constructor.
	 */
	StringCompare();
	virtual ~StringCompare();

	/**
	 * add a new regex string to ignore in the diff
	 * @param re : the string to regex ignore in the diff
	 */
	void addIgnoreRegex(const std::string& re);


	/**
	 * add a new regex string to the stop list
	 * @param re : the string which will stop the diff if match
	 */
	void addStopRegex(const std::string& re);

	/**
	 * Return the shortest execution sequence(SES) to obtain the destination string dst from the source src i.e. the diff
	 * @param src : source string
	 * @param dst : destination string
	 * @param output : the resulting SES diff in string representation
	 * @return : false if any stop flag has been matched, else true
	 */
	bool retrieveDiff(const std::string & src,const std::string& dst, std::string& output) const;
};

/**
 * BasicCompare implementation dedicated to the HTTP header comparaison
 * Handle Map comparaison and the regex stop and ignore its own way
 */
class StringCompareHeader : public StringCompare {
	typedef std::map<std::string,std::string> mapStrings;

	/**
	 * Concatenate the map in one string
	 */
	std::string mapToHeaderString(const mapStrings& map);
public:
	/**
	 * Call the super constructor.
	 */
	StringCompareHeader(const tStrings& stopRegex,const tStrings& ignoreRegex):StringCompare(stopRegex,ignoreRegex){}

	/**
	 * Call the super constructor.
	 */
	StringCompareHeader():StringCompare(){}


	/**
	 * Split both string on their newline caractere and return line by line diff
	 * @param src : source string
	 * @param dst : destination string
	 * @param output : the resulting SES diff in string representation
	 * @return : false if any stop flag has been matched, else true
	 */
	bool retrieveDiff(const std::string& src,const std::string& dst,std::string& output) const;
};

/**
 * BasicCompare implementation dedicated to the HTTP body comparaison
 * Handle vector of string comparaison and the regex stop and ignore its own way
 */
class StringCompareBody : public StringCompare {

public:
	/**
	 * Call the super constructor.
	 */
	StringCompareBody(tStrings stopRegex,tStrings ignoreRegex):StringCompare(stopRegex,ignoreRegex){}

	/**
	 * Call the super constructor.
	 */
	StringCompareBody():StringCompare(){}

	/**
	 * Split both string on their '><' junction and return the line by line diff
	 * Stop and Ignore regex are also process on the input strings
	 * @param src : the source string
	 * @param dst : the destination string
	 * @param output : the resulting SES diff in string representation
	 * @return : false if any stop flag has been matched, else true
	 */
	bool retrieveDiff(const std::string& src,const std::string& dst,std::string& output) const;
	/**
	 * Process the Ignore and Stop regex and every input strings
	 * @param src : source vector
	 * @param dst : destination vector
	 * @param output : the resulting SES diff in string representation
	 * @return : false if any stop flag has been matched, else true
	 */
	bool retrieveDiff(const tStrings& src,const tStrings& dst,std::string& output) const;
};


} /* namespace LibWsDiff */
