#include <iostream>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include "config.h"

namespace util {

bool Config::load(const std::string &filepath)
{
	mINI::INIFile file(filepath);

	return file.read(m_ini);
}

bool Config::save(const std::string &filepath)
{
	mINI::INIFile file(filepath);

	// Note that generate() will overwrite any custom formatting and comments from the original file!
	return file.generate(m_ini, true);
}

long Config::get_integer(const std::string &section, const std::string &key, long default_value)
{
	if (m_ini.has(section)) {
		if (m_ini[section].has(key)) {
			return std::stol(m_ini[section][key]);
		}
	}

	return default_value;
}

double Config::get_real(const std::string &section, const std::string &key, double default_value)
{
	if (m_ini.has(section)) {
		if (m_ini[section].has(key)) {
			return std::stod(m_ini[section][key]);
		}
	}

	return default_value;
}
	
bool Config::get_boolean(const std::string &section, const std::string &key, bool default_value)
{
	if (m_ini.has(section)) {
		if (m_ini[section].has(key)) {
			const auto &value = m_ini[section][key];
			if (value == "true" || value == "1") {
				return true;
			} else if (value == "false" || value == "0") {
				return false;
			}
		}
	}

	return default_value;
}

};
