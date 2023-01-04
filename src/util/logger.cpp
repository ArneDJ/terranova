#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>

#include "logger.h"

namespace logger {

static std::string g_log_header = "";
static std::vector<std::function<void(const std::string&)>> g_log_callbacks;
static std::mutex g_log_mutex;

void init(const std::string &header)
{
	g_log_header = header;

	std::cout << header << std::endl;
}

void exit(const std::string &footer)
{
	// write a closing message to each log sink
	for (auto &callback : g_log_callbacks) {
		callback(footer);
	}

	std::cout << footer << std::endl;

	// close all sinks
	g_log_callbacks.clear();
}

void add_sink(std::function<void(const std::string&)> callback)
{
	// add header
	callback(g_log_header);

	g_log_callbacks.push_back(callback);
}

void print_message(const std::string &message)
{
	std::lock_guard<std::mutex> guard(g_log_mutex);

	// to cout
	std::cout << message << std::endl;

	// to callbacks
	for (auto &callback : g_log_callbacks) {
		callback(message);
	}
}

}
