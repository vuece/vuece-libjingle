/*
 * VueceKeyValuePair.h
 *
 *  Created on: Mar 17, 2013
 *      Author: Jingjing Sun
 */

#ifndef VUECEKEYVALUEPAIR_H_
#define VUECEKEYVALUEPAIR_H_

#include <string>

namespace vuece
{
class VueceKeyValuePair {
public:
	VueceKeyValuePair(const std::string& k, const std::string& v);

	std::string Key() const;
	std::string Value() const;

private:
	std::string iKey;
	std::string iValue;
};

}



#endif /* VUECEKEYVALUEPAIR_H_ */
