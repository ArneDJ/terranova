#include <unordered_map>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>

#include "input.h"

namespace util {

Keymap InputManager::m_current;
Keymap InputManager::m_previous;
MouseCoords InputManager::m_mouse_coords;
bool InputManager::m_exit = false;
int InputManager::m_mousewheel = 0;

void InputManager::update()
{
	// first copy over keymap of tick before update to previous keymap
	for (auto &iter : m_current) {
		m_previous[iter.first] = iter.second;
	}

	m_mousewheel = 0;

	// now sample the current events
	SDL_Event event;
	while (SDL_PollEvent(&event)) { sample_event(&event); }

	// mouse coord sampling
	sample_absolute_mouse();
	sample_relative_mouse();	
}

bool InputManager::exit_request()
{
	return m_exit;
}
	
bool InputManager::key_down(uint32_t key)
{
	auto result = m_current.find(key);
	if (result != m_current.end()) {
		return result->second;
	}

	return false;
}
	
bool InputManager::key_pressed(uint32_t key)
{
	return (key_down(key) && !was_key_down(key));
}

bool InputManager::was_key_down(uint32_t key)
{
	auto result = m_previous.find(key);
	if (result != m_previous.end()) {
		return result->second;
	}

	return false;
}

glm::vec2 InputManager::abs_mouse_coords()
{
	return m_mouse_coords.absolute;
}

glm::vec2 InputManager::rel_mouse_coords()
{
	return m_mouse_coords.relative;
}
	
int InputManager::mousewheel()
{
	return m_mousewheel;
}

void InputManager::sample_event(const SDL_Event *event)
{
	if (event->type == SDL_QUIT) { m_exit = true; }

	if (event->type == SDL_KEYDOWN) { press_key(event->key.keysym.sym); }
	if (event->type == SDL_KEYUP) { release_key(event->key.keysym.sym); }

	if (event->type == SDL_MOUSEBUTTONDOWN) { press_key(event->button.button); }
	if (event->type == SDL_MOUSEBUTTONUP) { release_key(event->button.button); }

	if (event->type == SDL_MOUSEWHEEL) { m_mousewheel = event->wheel.y; }
}

void InputManager::press_key(uint32_t key)
{
	m_current[key] = true;
}

void InputManager::release_key(uint32_t key)
{
	m_current[key] = false;
}

void InputManager::sample_relative_mouse()
{
	int x = 0;
	int y = 0;

	SDL_GetRelativeMouseState(&x, &y);
	m_mouse_coords.relative.x = float(x);
	m_mouse_coords.relative.y = float(y);
/*
	if (mousegrab == false) {
		m_mouse_coords.relative.x = 0.f;
		m_mouse_coords.relative.y = 0.f;
	}
	*/
}

void InputManager::sample_absolute_mouse()
{
	int x = 0;
	int y = 0;

	SDL_GetMouseState(&x, &y);
	m_mouse_coords.absolute.x = float(x);
	m_mouse_coords.absolute.y = float(y);
}

};
