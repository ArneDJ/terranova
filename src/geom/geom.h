#pragma once 
#include <algorithm>

namespace geom {

template<typename T>
inline T max(const T &a, const T &b, const T &c)
{
	return std::max(a, std::max(b, c));
}

template<typename T>
inline T min(const T &a, const T &b, const T &c)
{
	return std::min(a, std::min(b, c));
}

};
