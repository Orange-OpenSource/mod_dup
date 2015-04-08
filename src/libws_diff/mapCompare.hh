/*
 * mapCompare.h
 *
 *  Created on: 11 fev. 2014
 *      Author: cvallee
 */

#pragma once

#include <vector>
#include <boost/regex.hpp>
#include "DiffPrinter/diffPrinter.hh"


typedef std::vector<boost::regex> tRegexes;
typedef std::vector<std::string> tStrings;

namespace LibWsDiff {

/**
 * MapCompare class
 * Interface providing tools for maps comparaison and handling ignore/stop key cases
 */
class MapCompare {

	typedef std::map<std::string,std::string> mapStrings;
	typedef std::map<std::string,boost::regex> mapKeyRegex;

	/*typedef bool (*stopFunction)(const std::string&);
	typedef void (*ignoreFunction)(std::string&);
	typedef std::map<std::string,stopFunction> mapStopRegex;
	typedef std::map<std::string,ignoreFunction> mapIgnoreRegex;*/

	//map of function returning true if key regex is matched
	mapKeyRegex mStopRegex;
	//Map of function replacing the ignore match
	mapKeyRegex mIgnoreRegex;

public:
	/**
	 * Default Constructor.
	 */
	MapCompare();
	virtual ~MapCompare();

	/**
	 * Add a new ignore regex for map diffing
	 * @param key : the key concerned by the new regex
	 * @param myregex : the string to ignore in any diff
	 */
	void addIgnoreRegex(const std::string& key,const std::string& myregex);

	/**
	 * Add a new stop regex for map diffing
	 * @param key : the key concerned by the new regex
	 * @param myregex : the string stopping the diff
	 */
	void addStopRegex(const std::string& key,const std::string& myregex);


	bool checkStop(const mapStrings& map) const;

	void applyIgnoreRegex(mapStrings& map) const;

	/**
	 * Provide the map differences between the two provided,
	 * @param src : the source map
	 * @param dst : the destination map
	 * @param output : the resulting diff in string format
	  * @return : false if any stop flag has been matched, else true
	 */
	bool retrieveDiff(const mapStrings& mapOrig,
			const mapStrings& mapRes,
			std::string& output) const;

	/**
	 * Provide the map differences between the two provided,
	 * @param src : the source map
	 * @param dst : the destination map
	 * @param printer : the display class
	  * @return : false if any stop flag has been matched, else true
	 */
	bool retrieveDiff(const mapStrings& mapOrig,
			const mapStrings& mapRes,
			LibWsDiff::diffPrinter& printer) const;
};

} /* namespace LibWsDiff */
