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
#include "../extern/imgui/imgui_impl_sdl.h"
#include "../extern/imgui/imgui_impl_opengl3.h"

#include "../util/input.h"
#include "../util/camera.h"
#include "../util/timer.h"
#include "../util/navigation.h"
#include "../util/image.h"
#include "../util/animation.h"
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
#include "../module.h"
#include "../media.h"

#include "battle.h"

const geom::AABB SCENE_BOUNDS = {
	{ 0.F, 0.F, 0.F },
	{ 1024.F, 255.F, 1024.F }
};
	
// TODO remove
glm::vec3 creature_fly_direction(const glm::vec3 &view, bool forward, bool backward, bool right, bool left)
{
	glm::vec3 direction = { 0.f, 0.f, 0.f };
	glm::vec3 dir = glm::normalize(view);
	if (forward) {
		direction = dir;
	}
	if (backward) {
		direction = -dir;
	}
	if (right) {
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x = tmp.x;
		direction.y = 0.f;
		direction.z = tmp.z;
	}
	if (left) {
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x = -tmp.x;
		direction.y = 0.f;
		direction.z = -tmp.z;
	}

	return direction;
}

glm::vec2 creature_direction(const glm::vec3 &view, bool forward, bool backward, bool right, bool left)
{
	glm::vec2 direction = { 0.f, 0.f };
	glm::vec3 dir = glm::normalize(view);
	if (forward) {
		direction.x += dir.x;
		direction.y += dir.z;
	}
	if (backward) {
		direction.x -= dir.x;
		direction.y -= dir.z;
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

void Battle::init(const gfx::ShaderGroup *shaders)
{
	debugger = std::make_unique<Debugger>(shaders->debug, shaders->debug, shaders->culling);

	scene = std::make_unique<gfx::SceneGroup>(shaders->debug, shaders->culling);
	scene->set_scene_type(gfx::SceneType::FIXED);

	terrain = std::make_unique<Terrain>(shaders->terrain, SCENE_BOUNDS);
	terrain->add_material("TILES", MediaManager::load_texture("modules/native/media/textures/terrain/tiles.dds"));

	landscaper.bounds = {
		{ SCENE_BOUNDS.min.x, SCENE_BOUNDS.min.z },
		{ SCENE_BOUNDS.max.x, SCENE_BOUNDS.max.z }
	};
	
	creature_shader = shaders->creature;
}

void Battle::load_molds(const Module &module)
{
	house_molds.clear();

	uint32_t id = 1; // TODO import ids from module

	for (const auto &module_house : module.houses) {
		for (const auto &model_path : module_house.models) {
			house_molds[id] = std::make_unique<HouseMold>(id, MediaManager::load_model(model_path));

			id++;
		}
	}

	// add house info to landscape
	for (const auto &bucket : house_molds) {
		const auto &mold = bucket.second;
		landscaper.add_house(mold->id, mold->model->bounds());
	}

	// load skeletons and animations
	// TODO import from module
	skeleton = MediaManager::load_skeleton("modules/native/media/skeletons/human.ozz");
	animation_idle = MediaManager::load_animation("modules/native/media/animations/human/idle.ozz");
	animation_run = MediaManager::load_animation("modules/native/media/animations/human/run.ozz");
	animation_falling = MediaManager::load_animation("modules/native/media/animations/human/falling.ozz");
}

void Battle::prepare(const BattleParameters &params)
{
	parameters = params;

	landscaper.clear();

	// local seed is based on campaign seed plus local tile index
	std::mt19937 gen(parameters.seed);
	gen.discard(parameters.tile);
	std::uniform_int_distribution<int> distrib;
	terrain->generate(distrib(gen));

	landscaper.generate(parameters.seed, parameters.tile, parameters.town_size);
	
	physics.add_object(terrain->height_field()->object());

	add_houses();

	add_creatures();

	// grab mouse
	mousegrab = true;
	SDL_SetRelativeMouseMode(SDL_TRUE);
	
	camera_mode = BattleCamMode::THIRD_PERSON;
}

void Battle::clear()
{
	scene->clear_instances();

	debugger->clear();

	// clear physical objects
	physics.clear_objects();

	// clear entities
	building_entities.clear();

	creature_entities.clear();
}

void Battle::update(float delta)
{
	if (util::InputManager::key_pressed(SDLK_ESCAPE)) { 
		mousegrab = !mousegrab;
		if (mousegrab) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
		} else {
			SDL_SetRelativeMouseMode(SDL_FALSE);
		}
	}

	// camera distance third person control
	int scroll = util::InputManager::mousewheel();

	// change state
	if (camera_mode == BattleCamMode::FIRST_PERSON && scroll < 0) {
		camera_mode = BattleCamMode::THIRD_PERSON;
	}

	const float CAM_SCROLL_SPEED = 10.F;
	if (scroll > 0) {
		camera_zoom -= scroll * CAM_SCROLL_SPEED * delta;
	} else if (scroll < 0) {
		camera_zoom += (-scroll) * CAM_SCROLL_SPEED * delta;
	}

	if (camera_mode == BattleCamMode::THIRD_PERSON && camera_zoom < 0.2f) {
		camera_mode = BattleCamMode::FIRST_PERSON;
	}

	camera_zoom = glm::clamp(camera_zoom, 0.1f, 5.f);

	rotate_camera(delta);

	//glm::vec3 direction = creature_fly_direction(camera.direction, util::InputManager::key_down(SDLK_w), util::InputManager::key_down(SDLK_s), util::InputManager::key_down(SDLK_d), util::InputManager::key_down(SDLK_a));
	glm::vec2 dir = creature_direction(camera.direction, util::InputManager::key_down(SDLK_w), util::InputManager::key_down(SDLK_s), util::InputManager::key_down(SDLK_d), util::InputManager::key_down(SDLK_a));
	glm::vec3 direction = { dir.x, 0.f, dir.y };

	if (camera_mode == BattleCamMode::FIRST_PERSON || camera_mode == BattleCamMode::THIRD_PERSON) {
		player->set_movement(direction, util::InputManager::key_down(SDLK_SPACE));
	}
	player->update_collision(physics.world(), delta);

	auto world = physics.world();
	for (auto &creature : creature_entities) {
		creature->set_movement(direction, util::InputManager::key_down(SDLK_SPACE));
		creature->update_collision(physics.world(), delta);
	}

	physics.update(delta);
		
	player->update_transform();

	for (auto &creature : creature_entities) {
		creature->update_transform();
	}
	
	if (player->current_animation == CA_IDLE) {
		player->update_animation(skeleton, animation_idle, delta);
	} else if (player->current_animation == CA_RUN) {
		player->update_animation(skeleton, animation_run, delta);
	} else if (player->current_animation == CA_FALLING) {
		player->update_animation(skeleton, animation_falling, delta);
	}

	position_camera(delta);

	debugger->update(camera);
}

void Battle::display()
{
	scene->update(camera);
	scene->cull_frustum();
	scene->display();

	creature_shader->use();
	creature_shader->uniform_mat4("CAMERA_VP", camera.VP);
	creature_shader->uniform_mat4("MODEL", player->transform->to_matrix());
	player->joint_matrices.buffer.bind_base(0);
	player->model->display();

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	terrain->display(camera);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	debugger->display();
}

float Battle::vertical_offset(float x, float z)
{
	glm::vec3 origin = { x, 2.F * SCENE_BOUNDS.max.y, z };
	glm::vec3 end = { x, 0.F, z };

	auto result = physics.cast_ray(origin, end);
	
	return result.point.y;
}
	
void Battle::update_debug_menu()
{
	ImGui::Begin("Battle Info");
	ImGui::SetWindowSize(ImVec2(300, 200));
	ImGui::Text("seed: %d", parameters.seed);
	ImGui::Text("tile: %d", parameters.tile);
	ImGui::Text("town size: %d", parameters.town_size);

	static bool freecam = false;
	ImGui::Checkbox("Freecam", &freecam);
	if (freecam) {
		camera_mode = BattleCamMode::FLYING_NOCLIP;
	} else {
		camera_mode = BattleCamMode::THIRD_PERSON;
	}

	ImGui::End();
}
	
void Battle::add_houses()
{
	for (const auto &house : landscaper.houses) {
		auto search = house_molds.find(house.second.mold_id);
		if (search != house_molds.end()) {
			auto object = scene->find_object(search->second->model);
			for (const auto &transform : house.second.transforms) {
				glm::vec3 position = { transform.position.x, 0.f, transform.position.y };
				position.y = vertical_offset(transform.position.x, transform.position.y);
				glm::quat rotation = glm::angleAxis(transform.angle, glm::vec3(0.f, 1.f, 0.f));
				auto building = std::make_unique<BuildingEntity>(position, rotation, search->second->collision->shape.get());

				object->add_transform(building->transform.get());

				building_entities.push_back(std::move(building));
			}
		}
	}

	// add collision
	for (const auto &building : building_entities) {
		physics.add_body(building->body.get());
	}
}

void Battle::add_creatures()
{
	player = std::make_unique<Creature>();
	glm::vec3 location = { 500.f, 255.f, 512.f };
	//location.y = vertical_offset(location.x, location.z) + 1.f;
	player->teleport(location);
	player->model = MediaManager::load_model("modules/native/media/models/human.glb");
	player->set_animation(skeleton, animation_idle);
	physics.add_object(player->bumper->ghost_object.get(), btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::AllFilter);
	//debugger->add_capsule(player->bumper->shape->getRadius(), player->bumper->shape->getHalfHeight(), player->bumper->transform.get());

	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 0; j++) {
			glm::vec3 position = { 512.f + (i+i), 64.f, 512.f + (j+j) };
			position.y = vertical_offset(position.x, position.z) + 1.f;
			auto creature = std::make_unique<Creature>();
			creature->teleport(position);
			physics.add_object(creature->bumper->ghost_object.get(), btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::AllFilter);
			debugger->add_capsule(creature->bumper->shape->getRadius(), creature->bumper->shape->getHalfHeight(), creature->bumper->transform.get());
			creature_entities.push_back(std::move(creature));
		}
	}
}
	
void Battle::rotate_camera(float delta)
{
	float modifier = 10.f * delta;
	if (mousegrab) {
		glm::vec2 offset = modifier * 0.05f * util::InputManager::rel_mouse_coords();
		camera.add_offset(offset);
	}
}
	
void Battle::position_camera(float delta)
{
	if (camera_mode == BattleCamMode::FIRST_PERSON) {
		camera.position = player->transform->position;
		camera.position.y += 1.8f;
	} else if (camera_mode == BattleCamMode::THIRD_PERSON) {
		camera.position = player->transform->position;
		camera.position.y += 1.5f;
		camera.position -= (camera_zoom * camera.direction);
	}

	float speed = 50.f * delta;
	if (camera_mode == BattleCamMode::FLYING_NOCLIP) {
		if (util::InputManager::key_down(SDLK_w)) { camera.move_forward(speed); }
		if (util::InputManager::key_down(SDLK_s)) { camera.move_backward(speed); }
		if (util::InputManager::key_down(SDLK_d)) { camera.move_right(speed); }
		if (util::InputManager::key_down(SDLK_a)) { camera.move_left(speed); }
	}

	camera.update_viewing();
}
