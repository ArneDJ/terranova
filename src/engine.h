
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

class Engine {
public:
	Engine();
	~Engine();
public:
	void run();
private:
	util::FrameTimer frame_timer;
	SDL_Window *window = nullptr;
	SDL_GLContext glcontext;
	UserDirectory user_dir;
	util::Config config;
	VideoSettings video_settings;
private:
	void init_imgui();
};
