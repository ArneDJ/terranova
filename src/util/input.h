namespace util {

using Keymap = std::unordered_map<uint32_t, bool>;

struct MouseCoords {
	glm::vec2 absolute = {};
	glm::vec2 relative = {};
};

class InputManager {
public:
	static void update();
public:
	static bool exit_request();
	static bool key_down(uint32_t key); // is key held down
	static bool key_pressed(uint32_t key); // is key pressed once
	static glm::vec2 abs_mouse_coords();
	static glm::vec2 rel_mouse_coords();
	static int mousewheel();
private:
	static Keymap m_current;
	static Keymap m_previous;
	static MouseCoords m_mouse_coords;
	static bool m_exit;
	static int m_mousewheel;
private:
	static void sample_event(const SDL_Event *event);
	static void sample_absolute_mouse();
	static void sample_relative_mouse();	
	static void press_key(uint32_t key);
	static void release_key(uint32_t key);
	static bool was_key_down(uint32_t key);
};

};
