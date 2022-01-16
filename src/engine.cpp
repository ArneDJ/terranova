#include <iostream>
#include <chrono>
#include <unordered_map>
#include <list>
#include <queue>
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
#include "extern/cereal/archives/json.hpp"

#include "extern/freetypegl/freetype-gl.h"

#include "engine.h"

static const char *GAME_NAME = "terranova";

// returns true if a user directory exists
bool UserDirectory::locate(const char *base)
{
	return (locate_dir(base, "settings", settings) && locate_dir(base, "saves", saves));
}

// finds the user data directory 
// returns true if found
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

	// get user paths where the ini files are located
	if (!user_dir.locate(GAME_NAME)) {
		LOG_F(FATAL, "Could not find user settings directory");
		exit(EXIT_FAILURE);
	}
	
	// load our user settings
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

	// get video settings
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
	MediaManager::clear();

	ConsoleManager::clear();

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

	// initialize glew
	GLenum error = glewInit();
	if (error != GLEW_OK) {
		LOG_F(FATAL, "Could not init GLEW: %s\n", glewGetErrorString(error));
		exit(EXIT_FAILURE);
	}

	// set some opengl settings
	glClearColor(0.5f, 0.5f, 0.8f, 1.f);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);
	glLineWidth(2.f);
	glEnable(GL_POLYGON_OFFSET_LINE);

	glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glPolygonOffset(-1.0f, -1.0f);
}

void Engine::load_shaders()
{
	shaders = std::make_unique<gfx::ShaderGroup>();

	shaders->debug = std::make_shared<gfx::Shader>();
	shaders->debug->compile("shaders/debug.vert", GL_VERTEX_SHADER);
	shaders->debug->compile("shaders/debug.frag", GL_FRAGMENT_SHADER);
	shaders->debug->link();

	shaders->object = std::make_shared<gfx::Shader>();
	shaders->object->compile("shaders/object.vert", GL_VERTEX_SHADER);
	shaders->object->compile("shaders/object.frag", GL_FRAGMENT_SHADER);
	shaders->object->link();

	shaders->tilemap = std::make_shared<gfx::Shader>();
	shaders->tilemap->compile("shaders/board.vert", GL_VERTEX_SHADER);
	shaders->tilemap->compile("shaders/board.tesc", GL_TESS_CONTROL_SHADER);
	shaders->tilemap->compile("shaders/board.tese", GL_TESS_EVALUATION_SHADER);
	shaders->tilemap->compile("shaders/board.frag", GL_FRAGMENT_SHADER);
	shaders->tilemap->link();

	shaders->culling = std::make_shared<gfx::Shader>();
	shaders->culling->compile("shaders/culling.comp", GL_COMPUTE_SHADER);
	shaders->culling->link();

	shaders->terrain = std::make_shared<gfx::Shader>();
	shaders->terrain->compile("shaders/terrain.vert", GL_VERTEX_SHADER);
	shaders->terrain->compile("shaders/terrain.tesc", GL_TESS_CONTROL_SHADER);
	shaders->terrain->compile("shaders/terrain.tese", GL_TESS_EVALUATION_SHADER);
	shaders->terrain->compile("shaders/terrain.frag", GL_FRAGMENT_SHADER);
	shaders->terrain->link();

	shaders->label = std::make_shared<gfx::Shader>();
	shaders->label->compile("shaders/label.vert", GL_VERTEX_SHADER);
	shaders->label->compile("shaders/label.frag", GL_FRAGMENT_SHADER);
	shaders->label->link();

	shaders->creature = std::make_shared<gfx::Shader>();
	shaders->creature->compile("shaders/creature.vert", GL_VERTEX_SHADER);
	shaders->creature->compile("shaders/creature.frag", GL_FRAGMENT_SHADER);
	shaders->creature->link();

	shaders->blur = std::make_shared<gfx::Shader>();
	shaders->blur->compile("shaders/blur.comp", GL_COMPUTE_SHADER);
	shaders->blur->link();
}
	
void Engine::load_module()
{
	Module module;
	module.load();

	// load battle molds from the module
	battle.load_molds(module);

	// load campaign blueprints from the module
	campaign.load_blueprints(module);
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
	
void Engine::update_campaign_menu()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);

	ImGui::NewFrame();
	ImGui::Begin("Campaign Debug");
	ImGui::SetWindowSize(ImVec2(400, 300));
	ImGui::Text("cam position: %f, %f, %f", campaign.camera.position.x, campaign.camera.position.y, campaign.camera.position.z);
	ImGui::Text("%.2f ms/frame (%.1d fps)", (frame_timer.FPS_UPDATE_TIME / frame_timer.frames_per_second()), frame_timer.frames_per_second());
	ImGui::Text("%.4f frame delta", frame_timer.delta_seconds());
	ImGui::Text("Game ticks: %d", campaign.game_ticks);
	ImGui::Checkbox("Show debug objects", &campaign.display_debug);
	ImGui::Checkbox("Show world wireframe", &campaign.wireframe_worldmap);
	ImGui::Separator();
	if (ImGui::Button("Save Game")) { 
		campaign.save(user_dir.saves + "test.save");
	}
	ImGui::Separator();
	if (ImGui::Button("Exit to Title")) { state = EngineState::TITLE; }
	if (ImGui::Button("Exit")) { state = EngineState::EXIT; }

	if (show_console) {
		ConsoleManager::display();
	}

	ImGui::End();
}

void Engine::run_campaign()
{
	state = EngineState::RUNNING_CAMPAIGN;

	// the campaign loop
	while (state == EngineState::RUNNING_CAMPAIGN) {
		frame_timer.begin(); // begin a new frame
	
		// first update user keyboard and mouse input
		util::InputManager::update();

		if (util::InputManager::key_pressed(SDLK_BACKQUOTE)) {
			show_console = !show_console;
		}

		update_campaign_menu();

		// internal campaign update
		campaign.update(frame_timer.delta_seconds());
	
		// after everything has updated render the frame
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, video_settings.canvas.x, video_settings.canvas.y);
		campaign.display();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);

		// go into battle mode if requested
		if (campaign.state == CampaignState::BATTLE_REQUEST) {
			run_battle();
			campaign.state = CampaignState::PAUSED;
		}
		if (util::InputManager::exit_request()) {
			state = EngineState::EXIT;
		}

		frame_timer.finish(); //  end frame
	}

	campaign.clear();
}

void Engine::run()
{
	state = EngineState::TITLE;

	load_shaders();

	load_module();

	// initialize the campaign
	campaign.init(shaders.get());
	campaign.camera.set_projection(video_settings.fov, video_settings.canvas.x, video_settings.canvas.y, 0.1f, 900.f);
	
	// initialize the battle
	battle.init(shaders.get());
	battle.camera.set_projection(video_settings.fov, video_settings.canvas.x, video_settings.canvas.y, 0.1f, 900.f);

	// random number for the world seed
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> distrib;

	// the main menu loop
	while (state == EngineState::TITLE) {
		util::InputManager::update(); // user keyboard and mouse input
		if (util::InputManager::exit_request()) {
			state = EngineState::EXIT;
		}

		update_main_menu();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		
		SDL_GL_SwapWindow(window);
			
		// start a new campaign
		if (state == EngineState::NEW_CAMPAIGN) {
			campaign.clear();
			campaign.generate(distrib(gen));
			//campaign.generate(1337);
			campaign.reset_camera();
			state = EngineState::RUNNING_CAMPAIGN;
		}
		// load a campaign from save file
		if (state == EngineState::LOADING_CAMPAIGN) {
			campaign.clear();
			campaign.load(user_dir.saves + "test.save");
			state = EngineState::RUNNING_CAMPAIGN;
		}
		// run the selected campaign
		if (state == EngineState::RUNNING_CAMPAIGN) {
			campaign.prepare();
			run_campaign();
		}
	}
}

void Engine::update_battle_menu()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);
	ImGui::NewFrame();
	ImGui::Begin("Battle Debug");
	ImGui::SetWindowSize(ImVec2(400, 300));
	ImGui::Text("cam position: %f, %f, %f", battle.camera.position.x, battle.camera.position.y, battle.camera.position.z);
	ImGui::Text("%.2f ms/frame (%.1d fps)", (frame_timer.FPS_UPDATE_TIME / frame_timer.frames_per_second()), frame_timer.frames_per_second());
	ImGui::Text("%.4f frame delta", frame_timer.delta_seconds());
	ImGui::Separator();
	if (ImGui::Button("Return to Campaign")) { state = EngineState::RUNNING_CAMPAIGN; }
	ImGui::Separator();
	if (ImGui::Button("Quit Game")) { state = EngineState::EXIT; }

	ImGui::End();

	battle.update_debug_menu();

	if (show_console) {
		ConsoleManager::display();
	}
}
	
void Engine::run_battle()
{
	ConsoleManager::print("starting new battle\n");

	state = EngineState::BATTLE; 

	// import local battle settings from campaign
	BattleParameters parameters;
	parameters.seed = campaign.seed;
	parameters.tile = campaign.battle_data.tile;
	parameters.town_size = campaign.battle_data.town_size;

	// prepare the battle based on selected parameters
	battle.prepare(parameters);

	// the battle loop
	while (state == EngineState::BATTLE) {
		frame_timer.begin(); // begin a new frame
	
		// first update keyboard and mouse input
		util::InputManager::update();

		if (util::InputManager::key_pressed(SDLK_BACKQUOTE)) {
			show_console = !show_console;
		}
		
		update_battle_menu();
		
		// internal battle update
		battle.update(frame_timer.delta_seconds());

		// render the battle
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, video_settings.canvas.x, video_settings.canvas.y);
		battle.display();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);

		if (util::InputManager::exit_request()) {
			state = EngineState::EXIT;
		}

		frame_timer.finish(); // end the frame
	}

	// battle has ended so clean everything up
	battle.clear();
}

void Engine::update_main_menu()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);
	ImGui::NewFrame();
	ImGui::Begin("Main Menu Debug Mode");
	ImGui::SetWindowSize(ImVec2(400, 200));
	if (ImGui::Button("New World")) {
		state = EngineState::NEW_CAMPAIGN;
	}
	ImGui::Separator();
	if (ImGui::Button("Load World")) {
		state = EngineState::LOADING_CAMPAIGN;
	}
	ImGui::Separator();
	if (ImGui::Button("Exit")) { state = EngineState::EXIT; }
	ImGui::End();
}
