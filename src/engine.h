#pragma once
#include "util/serialize.h"
#include "util/config.h"
#include "util/input.h"
#include "util/camera.h"
#include "util/timer.h"
#include "util/navigation.h"
#include "geometry/geometry.h"
#include "geometry/transform.h"
#include "geometry/voronoi.h"
#include "graphics/shader.h"
#include "graphics/mesh.h"
#include "graphics/model.h"
#include "graphics/scene.h"
#include "physics/physical.h"
#include "physics/heightfield.h"

#include "campaign/campaign.h"

#include "media.h"
#include "debugger.h"

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

class ShaderGroup {
public:
	gfx::Shader debug;
	gfx::Shader tilemap;
	gfx::Shader culling;
public:
	ShaderGroup();
};

enum class EngineState {
	TITLE,
	CAMPAIGN,
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
	std::unique_ptr<ShaderGroup> shaders;
private:
	void init_opengl();
	void init_imgui();
private:
	void update_debug_menu();
};
