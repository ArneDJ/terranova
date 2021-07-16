#include <iostream>

#include "extern/loguru/loguru.hpp"

int main(int argc, char *argv[])
{
	// initialize the logger
	loguru::init(argc, argv);
	loguru::add_file("events.log", loguru::Append, loguru::Verbosity_MAX);

	LOG_F(INFO, "Hello World!");
	LOG_F(INFO, "I'm hungry for some %.3f!", 3.14159);
	LOG_F(ERROR, "This is an error message");

	return 0;
}
