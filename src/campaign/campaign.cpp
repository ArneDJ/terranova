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

#include "../extern/cereal/archives/binary.hpp"

#include "../util/serialize.h"
#include "../util/config.h"
#include "../util/input.h"
#include "../util/camera.h"
#include "../util/timer.h"
#include "../util/navigation.h"
#include "../util/image.h"
#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../geometry/voronoi.h"
#include "../graphics/shader.h"
#include "../graphics/mesh.h"
#include "../graphics/model.h"
#include "../graphics/scene.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"
#include "../physics/trigger.h"

#include "../debugger.h"
#include "../media.h"

#include "campaign.h"

enum CampaignCollisionGroup {
	COLLISION_GROUP_NONE = 0,
	COLLISION_GROUP_RAY = 1 << 0,
	COLLISION_GROUP_INTERACTION = 1 << 1,
	COLLISION_GROUP_VISIBILITY = 1 << 2,
	COLLISION_GROUP_HEIGHTMAP = 1 << 3
};
	
void Campaign::init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tilemap)
{
	debugger = std::make_unique<Debugger>(tilemap, visual, culling);

	scene = std::make_unique<gfx::SceneGroup>(visual, culling);
	scene->set_scene_type(gfx::SceneType::DYNAMIC);
	
	board = std::make_unique<Board>(tilemap);
}
	
void Campaign::load(const std::string &filepath)
{
	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(name);
		board->load(archive);
		archive(camera);
		archive(player);
		archive(meeples);
	} else {
		LOG_F(ERROR, "Game loading error: could not open save file %s", filepath.c_str());
	}
}

void Campaign::save(const std::string &filepath)
{
	std::ofstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryOutputArchive archive(stream);
		archive(name);
		board->save(archive);
		archive(camera);
		archive(player);
		archive(meeples);
	} else {
		LOG_F(ERROR, "Game saving error: could not open save file %s", filepath.c_str());
	}
}
	
void Campaign::generate(int seed)
{
	board->generate(seed);

	player.teleport(glm::vec2(512.f));
	player.set_speed(10.f);
	player.set_name("Player Army");

	// add meeples
	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> dis_x(board->BOUNDS.min.x, board->BOUNDS.max.x);
	std::uniform_real_distribution<float> dis_y(board->BOUNDS.min.y, board->BOUNDS.max.y);
	
	for (int i = 0; i < 100; i++) {
		glm::vec2 point = { dis_x(gen), dis_y(gen) };
		const auto &tile = board->tile_at(point);
		if (tile) {
			if (tile->relief == ReliefType::LOWLAND || tile->relief == ReliefType::HILLS) {
				auto meeple = std::make_unique<Meeple>();
				meeple->teleport(point);
				meeple->set_name("Testificate Army");
				meeples.push_back(std::move(meeple));
			}
		}
	}
}
	
void Campaign::prepare()
{
	// prepare camera
	camera.position = player.transform()->position + glm::vec3(0.f, 10.f, -10.f);
	camera.target(player.transform()->position);
	
	// add grapics models
	
	board->reload();

	auto cone_model = MediaManager::load_model("media/models/primitives/cone.glb");
	auto cone_object = scene->find_object(cone_model);
	cone_object->add_transform(marker.transform());

	auto duck_model = MediaManager::load_model("media/models/duck.glb");
	auto duck_object = scene->find_object(duck_model);

	int meeple_mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_RAY;
	player.sync();
	duck_object->add_transform(player.transform());
	debugger->add_sphere(player.trigger()->form(), player.trigger()->transform());
	physics.add_object(player.trigger()->ghost_object(), COLLISION_GROUP_INTERACTION, meeple_mask);
	debugger->add_sphere(player.visibility()->form(), player.visibility()->transform());
	physics.add_object(player.visibility()->ghost_object(), COLLISION_GROUP_VISIBILITY, COLLISION_GROUP_INTERACTION);

	for (auto &meeple : meeples) {
		meeple->sync();
		duck_object->add_transform(meeple->transform());
		// debug trigger volumes
		auto trigger = meeple->trigger();
		debugger->add_sphere(trigger->form(), trigger->transform());
		physics.add_object(trigger->ghost_object(), COLLISION_GROUP_INTERACTION, meeple_mask);
		auto visibility = meeple->visibility();
		debugger->add_sphere(visibility->form(), visibility->transform());
		physics.add_object(visibility->ghost_object(), COLLISION_GROUP_VISIBILITY, COLLISION_GROUP_INTERACTION);
	}
	
	// add physical objects
	int group = COLLISION_GROUP_HEIGHTMAP;
	int mask = COLLISION_GROUP_RAY;
	physics.add_body(board->height_field().body(), group, mask);
}
	
void Campaign::clear()
{
	// first clear the model instances
	scene->clear_instances();

	debugger->clear();

	// clear physical objects
	physics.clear_objects();

	// clear entities
	meeples.clear();
}

void Campaign::update(float delta)
{
	physics.update_collision_only();

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

	if (util::InputManager::key_pressed(SDL_BUTTON_RIGHT)) {
		glm::vec3 ray = camera.ndc_to_ray(util::InputManager::abs_mouse_coords());
		auto result = physics.cast_ray(camera.position, camera.position + (1000.f * ray));
		if (result.hit) {
			marker.teleport(result.point);
			std::list<glm::vec2> nodes;
			
			board->find_path(glm::vec2(player.transform()->position.x, player.transform()->position.z), glm::vec2(result.point.x, result.point.z), nodes);
			player.set_path(nodes);
		}
	}

	player.update(delta);
	for (auto &meeple : meeples) {
		meeple->update(delta);
	}
	
	debugger->update(camera);
}
	
void Campaign::display()
{
	scene->update(camera);
	scene->cull_frustum();
	scene->display();

	board->display(camera);
	
	if (display_debug) {
		debugger->display_wireframe();
	}
}
