#include <iostream>
#include <unordered_map>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/loguru/loguru.hpp"

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_opengl3.h"

#include "util/config.h"
#include "util/input.h"
#include "util/camera.h"
#include "gpu/shader.h"
#include "gpu/mesh.h"
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

	video_settings.canvas.x = config.get_integer("video", "window_width", 640);
	video_settings.canvas.y = config.get_integer("video", "window_height", 480);
	video_settings.fov = config.get_real("video", "fov", 90.0);
	
	window = SDL_CreateWindow(GAME_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, video_settings.canvas.x, video_settings.canvas.y, SDL_WINDOW_OPENGL);
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

	init_imgui();
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
	
void Engine::init_imgui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(window, glcontext);
	ImGui_ImplOpenGL3_Init("#version 460");
}

void Engine::run()
{
	util::Camera camera;
	float ratio = float(video_settings.canvas.x) / float(video_settings.canvas.y);
	camera.set_projection(video_settings.fov, ratio, 0.1f, 900.f);
	camera.teleport(glm::vec3(10.f));
	camera.target(glm::vec3(0.f));

	glClearColor(0.5f, 0.5f, 0.5f, 1.f);

	gpu::Shader shader;
	shader.compile("shaders/triangle.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/triangle.frag", GL_FRAGMENT_SHADER);
	shader.link();

	gpu::CubeMesh cube_mesh(glm::vec3(-1.f, -1.f, -1.f), glm::vec3(1.f, 1.f, 1.f));
	for (int i = 0; i < 50; i++) {
		for (int j = 0; j < 50; j++) {
			for (int k = 0; k < 50; k++) {
				cube_mesh.add_transform(glm::vec3(float(3*i), float(3*j), float(3*k)));
			}
		}
	}
	cube_mesh.update_commands();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(-1.0f, -1.0f);

	while (util::InputManager::exit_request() == false) {
		util::InputManager::update();
		// FIXME it's getting a bit crowded over here
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);
		ImGui::NewFrame();
		ImGui::Begin("Debug Mode");
		ImGui::SetWindowSize(ImVec2(400, 200));
		//ImGui::Text("ms per frame: %d", timer.ms_per_frame);
		ImGui::Text("cam position: %f, %f, %f", camera.position.x, camera.position.y, camera.position.z);
		if (ImGui::Button("Exit")) { break; }
		ImGui::End();

		// update camera
		float modifier = 10.f * (1.f/60.f);
		if (util::InputManager::key_down(SDL_BUTTON_RIGHT)) {
			glm::vec2 offset = modifier * 0.05f * util::InputManager::rel_mouse_coords();
			camera.add_offset(offset);
		}
		if (util::InputManager::key_down(SDLK_w)) { camera.move_forward(modifier); }
		if (util::InputManager::key_down(SDLK_s)) { camera.move_backward(modifier); }
		if (util::InputManager::key_down(SDLK_d)) { camera.move_right(modifier); }
		if (util::InputManager::key_down(SDLK_a)) { camera.move_left(modifier); }
		camera.update_viewing();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, video_settings.canvas.x, video_settings.canvas.y);

		const glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.5f)), 0.001f*SDL_GetTicks(), glm::vec3(1.0f, 1.0f, 1.0f));

		shader.use();
		shader.uniform_mat4("VP", camera.VP);

		shader.uniform_bool("WIRED_MODE", false);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		cube_mesh.draw();

		shader.uniform_bool("WIRED_MODE", true);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		cube_mesh.draw();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);
	}

}