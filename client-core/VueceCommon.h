/*
 * VueceCommon.h
 *
 *  Created on: 2014-9-10
 *      Author: Jingjing Sun
 */

#ifndef VUECECOMMON_H_
#define VUECECOMMON_H_

#include "VueceConstants.h"
#include <string>

class VueceCommon {
public:
	static void ConfigureLogging(int logging_level);
	static std::string CalculateMD5FromFile(const std::string& path);
	static void LogVueceEvent(VueceEvent e);


};

#endif /* VUECECOMMON_H_ */
