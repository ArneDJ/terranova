#pragma once
#include <functional>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace logger {

void init(const std::string &header);

void exit(const std::string &footer);

void add_sink(std::function<void(const std::string&)> callback);

void print_message(const std::string &message);

template<typename... A>
void print_formatted(fmt::format_string<A...> format, A &&... args)
{
	std::string message = fmt::vformat(format, fmt::make_format_args(args...));

	print_message(message);
}

template<typename... A>
void print_formatted_extra(const char* logtype, const char* file, int line, fmt::format_string<A...> format, A &&... args)
{
	std::string info = fmt::format("{}:{}:{} ", logtype, file, line);
	std::string message = fmt::vformat(format, fmt::make_format_args(args...));

	print_message(info + message);
}

// extract the file from the source full file path
#define SRC_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define INFO(...) print_formatted(__VA_ARGS__)

#define ERROR(...) print_formatted_extra("ERROR", SRC_FILENAME, __LINE__, __VA_ARGS__)

#define FATAL(...) print_formatted_extra("FATAL", SRC_FILENAME, __LINE__, __VA_ARGS__)

}
