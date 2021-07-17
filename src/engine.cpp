#include <iostream>
#include <unordered_map>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>

#include "extern/loguru/loguru.hpp"

#include "util/config.h"
#include "util/input.h"
#include "gpu/shader.h"
#include "engine.h"

static const char *GAME_NAME = "terranova";

bool UserDirectory::locate(const char *base)
{
	return (locate_dir(base, "settings", settings) && locate_dir(base, "saves", saves));
}

bool UserDirectory::locate_dir(const char *base, const char *target, std::string &output)
{
	char *pref_path = SDL_GetPrefPath(base, target);
	if (pref_path) {
		output = pref_path;

		SDL_free(pref_path);

		return true;
	}

	return false;
}

Engine::Engine()
{
	SDL_Init(SDL_INIT_VIDEO);

	// get user paths
	if (!user_dir.locate(GAME_NAME)) {
		LOG_F(FATAL, "Could not find user settings directory");
		exit(EXIT_FAILURE);
	}
	
	// load user settings
	std::string settings_filepath = user_dir.settings + "config.ini";
	if (!config.load(settings_filepath)) {
		if (config.load("default.ini")) {
			// save default settings to user path
			if (!config.save(settings_filepath)) {
				LOG_F(ERROR, "Could not save user settings %s", settings_filepath);
			}
		} else {
			LOG_F(ERROR, "Could not open default settings!");
		}
	}

	int canvas_width = config.get_integer("video", "window_width", 640);
	int canvas_height = config.get_integer("video", "window_height", 480);
	
	window = SDL_CreateWindow(GAME_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, canvas_width, canvas_height, SDL_WINDOW_OPENGL);
	// Check that the window was successfully created
	if (window == nullptr) {
		// In the case that the window could not be made...
		LOG_F(FATAL, "Could not create window: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (config.get_boolean("video", "fullscreen", false)) {
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
	}

	// initialize the OpenGL context
	glcontext = SDL_GL_CreateContext(window);

	GLenum error = glewInit();
	if (error != GLEW_OK) {
		LOG_F(FATAL, "Could not init GLEW: %s\n", glewGetErrorString(error));
		exit(EXIT_FAILURE);
	}
}

Engine::~Engine()
{
	// in reverse order of initialization
	SDL_GL_DeleteContext(glcontext);
	// Close and destroy the window
	SDL_DestroyWindow(window);

	// Clean up SDL
	SDL_Quit();
}
	
void Engine::run()
{
	glClearColor(0.5f, 0.5f, 0.5f, 1.f);

	gpu::Shader shader;
	shader.compile("shaders/triangle.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/triangle.frag", GL_FRAGMENT_SHADER);
	shader.link();

	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	while (util::InputManager::exit_request() == false) {
		util::InputManager::update();

		glClear(GL_COLOR_BUFFER_BIT);

		glViewport(0, 0, 640, 480);
		glClear(GL_COLOR_BUFFER_BIT);
		shader.use();
		glDrawArrays(GL_TRIANGLES, 0, 3);

		SDL_GL_SwapWindow(window);
	}

}
