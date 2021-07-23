#pragma once
#include "util/config.h"
#include "util/input.h"
#include "util/camera.h"
#include "util/timer.h"
#include "geom/transform.h"
#include "geom/geom.h"
#include "gpu/shader.h"
#include "gpu/mesh.h"
#include "gpu/model.h"
#include "gpu/scene.h"

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
	util::Camera camera;
private:
	void init_opengl();
	void init_imgui();
private:
	void update_state();
};
