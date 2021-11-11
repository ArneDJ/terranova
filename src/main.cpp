#include <iostream>
#include <list>
#include <queue>
#include <chrono>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_opengl3.h"

#include "extern/loguru/loguru.hpp"

#include "extern/freetypegl/freetype-gl.h"

#include "engine.h"

int main(int argc, char *argv[])
{
	// initialize the logger
	loguru::init(argc, argv);
	loguru::add_file("events.log", loguru::Append, loguru::Verbosity_MAX);

	Engine engine;
	engine.run();

	return 0;
}
