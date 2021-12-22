#pragma once
#include "util/serialize.h"
#include "util/config.h"
#include "util/input.h"
#include "util/camera.h"
#include "util/timer.h"
#include "util/navigation.h"
#include "util/animation.h"
#include "geometry/geometry.h"
#include "geometry/transform.h"
#include "geometry/voronoi.h"
#include "util/image.h"
#include "graphics/shader.h"
#include "graphics/mesh.h"
#include "graphics/texture.h"
#include "graphics/model.h"
#include "graphics/scene.h"
#include "graphics/label.h"
#include "physics/physical.h"
#include "physics/heightfield.h"
#include "physics/trigger.h"
#include "physics/bumper.h"

#include "console.h"
#include "debugger.h"
#include "media.h"
#include "module.h"

#include "campaign/campaign.h"

#include "battle/battle.h"

struct VideoSettings {
	glm::ivec2 canvas = {};
	float fov = 90.f;
};

// locates the user (saves, settings) directories with SDL
class UserDirectory {
public:
	std::string settings = "";
	std::string saves = "";
public:
	bool locate(const char *base);
private:
	bool locate_dir(const char *base, const char *target, std::string &output);
};

enum class EngineState {
	TITLE,
	NEW_CAMPAIGN,
	LOADING_CAMPAIGN,
	RUNNING_CAMPAIGN,
	BATTLE,
	EXIT
};

class Engine {
public:
	Engine();
	~Engine();
public:
	void run();
private:
	EngineState state = EngineState::TITLE;
	util::FrameTimer frame_timer;
	SDL_Window *window = nullptr;
	SDL_GLContext glcontext;
	UserDirectory user_dir;
	util::Config config;
	VideoSettings video_settings;
	Campaign campaign;
	Battle battle;
	std::unique_ptr<gfx::ShaderGroup> shaders;
private:
	bool show_console = false;
private:
	void init_opengl();
	void init_imgui();
	void load_shaders();
	void load_module();
	void update_main_menu();
private:
	void run_campaign();
	void update_campaign_menu();
private:
	void run_battle();
	void update_battle_menu();
};
