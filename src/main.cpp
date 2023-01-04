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

#include "extern/freetypegl/freetype-gl.h"

#include "engine.h"

int main(int argc, char *argv[])
{
	// start up the engine and run it
	Engine engine;
	engine.run();

	return 0;
}
