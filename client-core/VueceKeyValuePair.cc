/*
 * VueceKeyValuePair.cc
 *
 *  Created on: Mar 17, 2013
 *      Author: Jingjing Sun
 */

#include "VueceKeyValuePair.h"

namespace vuece
{

VueceKeyValuePair::VueceKeyValuePair(const std::string& k, const std::string& v)
{
	iKey = k;
	iValue = v;
}

std::string VueceKeyValuePair::Key() const {
  return iKey;
}

std::string VueceKeyValuePair::Value() const {
  return iValue;
}

}

