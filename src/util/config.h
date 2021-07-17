#pragma once
#include "../extern/minih/ini.h"

namespace util {

class Config {
public:
	bool load(const std::string &filepath);
	bool save(const std::string &filepath);
public:
	long get_integer(const std::string &section, const std::string &key, long default_value = 0);
	double get_real(const std::string &section, const std::string &key, double default_value = 0.0);
	bool get_boolean(const std::string &section, const std::string &key, bool default_value = false);
private:
	mINI::INIStructure m_ini; // structure that holds the data
};

};
