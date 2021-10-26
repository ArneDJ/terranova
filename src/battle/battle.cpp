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
	{ 1024.F, 32.F, 1024.F }
};
	
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

Player::Player()
{
	btTransform startTransform;
	startTransform.setIdentity ();
	startTransform.setOrigin(btVector3(512, 64, 512));

	capsule = new btCapsuleShapeZ(0.5, 1);

	ghost_object = new btPairCachingGhostObject();
	ghost_object->setWorldTransform(startTransform);
	ghost_object->setCollisionShape(capsule);
	ghost_object->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

	char_con = new btKinematicCharacterController(ghost_object, capsule, 0.05f);
	char_con->setGravity(btVector3(0.f, -9.81f, 0.f));
	char_con->setMaxJumpHeight(1.5);
	char_con->setMaxPenetrationDepth(0.2f);
	char_con->setMaxSlope(btRadians(60.0));
	char_con->setUp(btVector3(0, 1, 0));

	transform = std::make_unique<geom::Transform>();
}
	
void Player::update(const glm::vec3 &direction, bool jump_request)
{
	if (jump_request && char_con->onGround()) {
		char_con->jump(btVector3(0,6,0));
	}

	if (direction != glm::vec3(0,0,0)) {
		char_con->setWalkDirection(btVector3(direction.x , direction.y , direction.z).normalized()/10);
	} else {
		char_con->setWalkDirection(btVector3(0 , 0 , 0));
	}
}
	
void Player::update_transform()
{
	btTransform t = char_con->getGhostObject()->getWorldTransform();
	//btVector3 pos = t.getOrigin();

	transform->position = fysx::bt_to_vec3(t.getOrigin());
	//transform->rotation = fysx::bt_to_quat(t.getRotation());
}

void Battle::init(const gfx::ShaderGroup *shaders)
{
	debugger = std::make_unique<Debugger>(shaders->debug, shaders->debug, shaders->culling);

	scene = std::make_unique<gfx::SceneGroup>(shaders->debug, shaders->culling);
	scene->set_scene_type(gfx::SceneType::DYNAMIC);

	terrain = std::make_unique<Terrain>(shaders->terrain, SCENE_BOUNDS);

	landscaper.bounds = {
		{ SCENE_BOUNDS.min.x, SCENE_BOUNDS.min.z },
		{ SCENE_BOUNDS.max.x, SCENE_BOUNDS.max.z }
	};
	
	player = std::make_unique<Player>();
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
}

void Battle::prepare(const BattleParameters &params)
{
	parameters = params;

	landscaper.clear();

	terrain->generate(parameters.seed);

	landscaper.generate(parameters.seed, parameters.tile, parameters.town_size);
	
	physics.add_object(terrain->height_field()->object());

	add_houses();

	auto transform = std::make_unique<geom::Transform>();
	transform->position = glm::vec3(512.f, 64.f, 512.f);

	camera.position = glm::vec3(5.f, 5.f, 5.f);
	camera.target(transform->position);

	//player = std::make_unique<fysx::Bumper>(glm::vec3(512.f, 64.f, 512.f), 0.3f, 1.8f);
	//physics.add_body(player->body());

	//debugger->add_capsule(player->capsule_radius(), player->capsule_height(), player->transform());
	physics.add_object(player->ghost_object, btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::AllFilter);
	debugger->add_capsule(0.5f, 1.f, player->transform.get());
}

void Battle::clear()
{
	scene->clear_instances();

	debugger->clear();

	// clear physical objects
	physics.clear_objects();

	// clear entities
	building_entities.clear();
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

	/*
	player->set_velocity(10.f*direction.x, 10.f*direction.y);

	if (util::InputManager::key_down(SDLK_SPACE)) {
		player->jump();
	}
	*/
	player->char_con->preStep(physics.world());

	player->update(glm::vec3(direction.x, 0.f, direction.y), util::InputManager::key_down(SDLK_SPACE));

	player->char_con->playerStep(physics.world(), delta);

	physics.update(delta);
		
	player->update_transform();
	/*
	player->update(physics.world());

	glm::vec3 position = player->standing_position();
	camera.position = position - (5.f * camera.direction);
	*/
	camera.position = player->transform->position;
	//camera.position.y += 1.f;
	camera.position -= (5.f * camera.direction);

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
