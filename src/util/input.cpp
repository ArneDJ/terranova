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

	// TODO add mouse coord sampling
}

bool InputManager::exit_request()
{
	return m_exit;
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

};
