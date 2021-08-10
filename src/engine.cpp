#include <iostream>
#include <chrono>
#include <unordered_map>
#include <list>
#include <memory>
#include <random>
#include <fstream>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/loguru/loguru.hpp"

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_opengl3.h"

#include "extern/cereal/archives/binary.hpp"

#include "engine.h"

static bool g_generate = false;
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

ShaderGroup::ShaderGroup()
{
	debug.compile("shaders/debug.vert", GL_VERTEX_SHADER);
	debug.compile("shaders/debug.frag", GL_FRAGMENT_SHADER);
	debug.link();

	tilemap.compile("shaders/tilemap.vert", GL_VERTEX_SHADER);
	tilemap.compile("shaders/tilemap.frag", GL_FRAGMENT_SHADER);
	tilemap.link();

	culling.compile("shaders/culling.comp", GL_COMPUTE_SHADER);
	culling.link();
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
				LOG_F(ERROR, "Could not save user settings %s", settings_filepath.c_str());
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

	init_opengl();

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
	
void Engine::init_opengl()
{
	// initialize the OpenGL context
	glcontext = SDL_GL_CreateContext(window);

	GLenum error = glewInit();
	if (error != GLEW_OK) {
		LOG_F(FATAL, "Could not init GLEW: %s\n", glewGetErrorString(error));
		exit(EXIT_FAILURE);
	}

	glClearColor(0.5f, 0.5f, 0.5f, 1.f);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);
	glLineWidth(2.f);
	glEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(-1.0f, -1.0f);
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
	
void Engine::update_debug_menu()
{
	g_generate = false;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);
	ImGui::NewFrame();
	ImGui::Begin("Debug Mode");
	ImGui::SetWindowSize(ImVec2(400, 300));
	ImGui::Text("cam position: %f, %f, %f", campaign.camera.position.x, campaign.camera.position.y, campaign.camera.position.z);
	ImGui::Text("%.2f ms/frame (%.1d fps)", (frame_timer.FPS_UPDATE_TIME / frame_timer.frames_per_second()), frame_timer.frames_per_second());
	ImGui::Text("%.4f frame delta", frame_timer.delta_seconds());
	ImGui::Separator();
	if (ImGui::Button("Generate World")) { g_generate = true; }
	if (ImGui::Button("Save World")) { 
		campaign.save(user_dir.saves + "test.save");
	}
	if (ImGui::Button("Exit")) { state = EngineState::EXIT; }
	ImGui::End();
}

void Engine::run()
{
	state = EngineState::TITLE;

	shaders = std::make_unique<ShaderGroup>();

	Debugger debugger = Debugger(&shaders->debug, &shaders->culling);

	campaign.init(&shaders->debug, &shaders->culling, &shaders->tilemap);
	campaign.camera.set_projection(video_settings.fov, video_settings.canvas.x, video_settings.canvas.y, 0.1f, 900.f);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> distrib;

	campaign.load(user_dir.saves + "test.save");
	campaign.world->reload();

	while (state == EngineState::TITLE) {
		frame_timer.begin();
	
		util::InputManager::update();

		update_debug_menu();

		campaign.update(frame_timer.delta_seconds());
	
		if (g_generate) {
			campaign.generate(distrib(gen));
			//campaign.world->generate(distrib(gen));
			campaign.world->reload();
		}

		debugger.update(campaign.camera);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, video_settings.canvas.x, video_settings.canvas.y);

		campaign.display();

		debugger.display();
		debugger.display_wireframe();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);

		frame_timer.finish();

		if (util::InputManager::exit_request()) {
			state = EngineState::EXIT;
		}
	}
}
