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

#include "../extern/loguru/loguru.hpp"

#include "../extern/imgui/imgui.h"

#include "../util/input.h"
#include "../util/camera.h"
#include "../util/timer.h"
#include "../util/navigation.h"
#include "../util/image.h"
#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../graphics/shader.h"
#include "../graphics/mesh.h"
#include "../graphics/model.h"
#include "../graphics/scene.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"

#include "../debugger.h"
#include "../media.h"

#include "battle.h"

void Battle::init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tesselation)
{
	scene = std::make_unique<gfx::SceneGroup>(visual, culling);
	scene->set_scene_type(gfx::SceneType::DYNAMIC);

	terrain = std::make_unique<Terrain>(tesselation);

	transform = std::make_unique<geom::Transform>();
	transform->position = glm::vec3(0.f, 0.f, 0.f);

	auto model = MediaManager::load_model("media/models/dragon.glb");
	auto object = scene->find_object(model);
	object->add_transform(transform.get());

	camera.position = glm::vec3(5.f, 5.f, 5.f);
	camera.target(transform->position);
}

void Battle::prepare(int seed)
{
	terrain->generate(seed);
}

void Battle::update(float delta)
{
	// update camera
	float modifier = 10.f * delta;
	float speed = 10.f * modifier;
	if (util::InputManager::key_down(SDL_BUTTON_LEFT)) {
		glm::vec2 offset = modifier * 0.05f * util::InputManager::rel_mouse_coords();
		camera.add_offset(offset);
	}
	if (util::InputManager::key_down(SDLK_w)) { camera.move_forward(speed); }
	if (util::InputManager::key_down(SDLK_s)) { camera.move_backward(speed); }
	if (util::InputManager::key_down(SDLK_d)) { camera.move_right(speed); }
	if (util::InputManager::key_down(SDLK_a)) { camera.move_left(speed); }

	camera.update_viewing();
}

void Battle::display()
{
	scene->update(camera);
	scene->cull_frustum();
	scene->display();

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	terrain->display(camera);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
