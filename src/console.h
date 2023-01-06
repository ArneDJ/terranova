#pragma once 
#include <string>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

class Console {
public:
	static void clear();
	static void print_line(const std::string &line);
	template<typename... A>
	static void print(fmt::format_string<A...> format, A &&... args)
	{
		std::string message = fmt::vformat(format, fmt::make_format_args(args...));

		print_line(message);
	}
	static void display();
private:
	static std::vector<std::string> m_lines;
	static bool m_auto_scroll;
};
