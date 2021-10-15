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
#include "../graphics/texture.h"
#include "../graphics/model.h"
#include "../graphics/scene.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"
#include "../physics/bumper.h"

#include "../debugger.h"
#include "../media.h"

#include "battle.h"

// TODO remove
glm::vec2 creature_control(const glm::vec3 &view, bool forward, bool backward, bool right, bool left)
{
	glm::vec2 direction = {0.f, 0.f};
	glm::vec2 dir = glm::normalize(glm::vec2(view.x, view.z));
	if (forward) {
		direction.x += dir.x;
		direction.y += dir.y;
	}
	if (backward) {
		direction.x -= dir.x;
		direction.y -= dir.y;
	}
	if (right) {
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x += tmp.x;
		direction.y += tmp.z;
	}
	if (left) {
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x -= tmp.x;
		direction.y -= tmp.z;
	}

	return direction;
}

void Battle::init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tesselation)
{
	debugger = std::make_unique<Debugger>(visual, visual, culling);

	scene = std::make_unique<gfx::SceneGroup>(visual, culling);
	scene->set_scene_type(gfx::SceneType::DYNAMIC);

	terrain = std::make_unique<Terrain>(tesselation);

	transform = std::make_unique<geom::Transform>();
	transform->position = glm::vec3(512.f, 64.f, 512.f);

	auto model = MediaManager::load_model("modules/native/media/models/dragon.glb");
	auto object = scene->find_object(model);
	object->add_transform(transform.get());

	camera.position = glm::vec3(5.f, 5.f, 5.f);
	camera.target(transform->position);
	
	physics.add_object(terrain->height_field()->object());

	player = std::make_unique<fysx::Bumper>(glm::vec3(512.f, 64.f, 512.f), 0.3f, 1.8f);
	physics.add_body(player->body());

	debugger->add_capsule(player->capsule_height(), player->capsule_radius(), player->transform());
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
	/*
	if (util::InputManager::key_down(SDLK_w)) { camera.move_forward(speed); }
	if (util::InputManager::key_down(SDLK_s)) { camera.move_backward(speed); }
	if (util::InputManager::key_down(SDLK_d)) { camera.move_right(speed); }
	if (util::InputManager::key_down(SDLK_a)) { camera.move_left(speed); }
	*/
	glm::vec2 direction = creature_control(camera.direction, util::InputManager::key_down(SDLK_w), util::InputManager::key_down(SDLK_s), util::InputManager::key_down(SDLK_d), util::InputManager::key_down(SDLK_a));

	player->set_velocity(10.f*direction.x, 10.f*direction.y);

	if (util::InputManager::key_down(SDLK_SPACE)) {
		player->jump();
	}

	physics.update(delta);
		
	player->update(physics.world());

	glm::vec3 position = player->standing_position();
	camera.position = position - (5.f * camera.direction);
	//camera.position = position;

	camera.update_viewing();

	debugger->update(camera);
}

void Battle::display()
{
	scene->update(camera);
	scene->cull_frustum();
	scene->display();

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	terrain->display(camera);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	debugger->display();
}
