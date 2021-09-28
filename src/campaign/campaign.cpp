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
	debugger = std::make_unique<Debugger>(visual, visual, culling);

	scene = std::make_unique<gfx::SceneGroup>(visual, culling);
	scene->set_scene_type(gfx::SceneType::DYNAMIC);
	
	board = std::make_unique<Board>(tilemap);

	meeple_controller.player = std::make_unique<Meeple>();
}
	
void Campaign::load(const std::string &filepath)
{
	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(name);
		board->load(archive);
		archive(camera);
		archive(meeple_controller.player);
		archive(meeple_controller.meeples);
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
		archive(meeple_controller.player);
		archive(meeple_controller.meeples);
	} else {
		LOG_F(ERROR, "Game saving error: could not open save file %s", filepath.c_str());
	}
}
	
void Campaign::generate(int seed)
{
	board->generate(seed);

	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> dis_x(board->BOUNDS.min.x, board->BOUNDS.max.x);
	std::uniform_real_distribution<float> dis_y(board->BOUNDS.min.y, board->BOUNDS.max.y);
	std::uniform_int_distribution<uint8_t> color_dist;
	// spawn factions
	for (int i = 0; i < 100; i++) {
		glm::vec2 point = { dis_x(gen), dis_y(gen) };
		const auto &tile = board->tile_at(point);
		if (tile) {
			if (tile->relief == ReliefType::LOWLAND || tile->relief == ReliefType::HILLS) {

				auto search = faction_controller.tile_owners.find(tile->index);
				if (search == faction_controller.tile_owners.end()) {

					auto faction = std::make_unique<Faction>(faction_controller.factions.size() + 1);
					
					FactionColor color = { color_dist(gen), color_dist(gen), color_dist(gen) };
					faction->set_color(color);
					faction_controller.tile_owners[tile->index] = faction->ID();
					faction_controller.factions.push_back(std::move(faction));
				}
			}
		}
	}

	meeple_controller.player = std::make_unique<Meeple>();
	meeple_controller.player->teleport(glm::vec2(512.f));
	meeple_controller.player->set_speed(10.f);
	meeple_controller.player->set_name("Player Army");

	// add meeples
	
	for (int i = 0; i < 100; i++) {
		glm::vec2 point = { dis_x(gen), dis_y(gen) };
		const auto &tile = board->tile_at(point);
		if (tile) {
			if (tile->relief == ReliefType::LOWLAND || tile->relief == ReliefType::HILLS) {
				auto meeple = std::make_unique<Meeple>();
				meeple->teleport(point);
				meeple->set_name("Testificate Army");
				meeple_controller.meeples.push_back(std::move(meeple));
			}
		}
	}
}
	
void Campaign::prepare()
{
	// prepare camera
	camera.position = meeple_controller.player->transform()->position + glm::vec3(0.f, 10.f, -10.f);
	camera.target(meeple_controller.player->transform()->position);

	meeple_controller.player->sync();
	for (auto &meeple : meeple_controller.meeples) {
		meeple->sync();
	}
	
	prepare_collision();

	prepare_graphics();
}

void Campaign::prepare_collision()
{
	int meeple_mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_VISIBILITY | COLLISION_GROUP_RAY;

	physics.add_object(meeple_controller.player->trigger()->ghost_object(), COLLISION_GROUP_INTERACTION, meeple_mask);
	physics.add_object(meeple_controller.player->visibility()->ghost_object(), COLLISION_GROUP_VISIBILITY, COLLISION_GROUP_INTERACTION);

	for (auto &meeple : meeple_controller.meeples) {
		auto trigger = meeple->trigger();
		physics.add_object(trigger->ghost_object(), COLLISION_GROUP_INTERACTION, meeple_mask);
		auto visibility = meeple->visibility();
		physics.add_object(visibility->ghost_object(), COLLISION_GROUP_VISIBILITY, COLLISION_GROUP_INTERACTION);
	}
	
	// add physical objects
	int group = COLLISION_GROUP_HEIGHTMAP;
	int mask = COLLISION_GROUP_RAY;
	physics.add_body(board->height_field().body(), group, mask);
}

void Campaign::prepare_graphics()
{
	board->reload();

	auto cone_model = MediaManager::load_model("media/models/primitives/cone.glb");
	auto cone_object = scene->find_object(cone_model);
	cone_object->add_transform(marker.transform());

	auto duck_model = MediaManager::load_model("media/models/duck.glb");
	auto duck_object = scene->find_object(duck_model);

	duck_object->add_transform(meeple_controller.player->transform());
	debugger->add_sphere(meeple_controller.player->trigger()->form(), meeple_controller.player->trigger()->transform());
	debugger->add_sphere(meeple_controller.player->visibility()->form(), meeple_controller.player->visibility()->transform());

	for (auto &meeple : meeple_controller.meeples) {
		duck_object->add_transform(meeple->transform());
		// debug trigger volumes
		auto trigger = meeple->trigger();
		debugger->add_sphere(trigger->form(), trigger->transform());
		auto visibility = meeple->visibility();
		debugger->add_sphere(visibility->form(), visibility->transform());
	}

	// color map factions
	for (auto &tile : faction_controller.tile_owners) {
		uint32_t ID = tile.first;
		auto faction_color = faction_controller.factions[tile.second - 1]->color(); // FIXME use lookup
		glm::vec3 color = { faction_color.red / 255.f, faction_color.green / 255.f, faction_color.blue / 255.f };
		board->color_tile(ID, color);
	}

	board->update_model();
}
	
void Campaign::clear()
{
	// first clear the model instances
	scene->clear_instances();

	debugger->clear();

	// clear physical objects
	physics.clear_objects();

	// clear entities
	meeple_controller.clear();
	meeple_controller.player = std::make_unique<Meeple>();
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
			glm::vec2 hitpoint = glm::vec2(result.point.x, result.point.z);
			std::list<glm::vec2> nodes;
			
			board->find_path(glm::vec2(meeple_controller.player->position()), hitpoint, nodes);
			meeple_controller.player->set_path(nodes);
		}
	}

	meeple_controller.update(delta);

	if (meeple_controller.chases.size() > 0 && meeple_controller.chase_time_slice == 0.f) {
		auto &chase = *meeple_controller.current_chase;
		std::list<glm::vec2> nodes;
		board->find_path(chase->chaser->position(), chase->target->position(), nodes);
		chase->chaser->set_path(nodes);
	}

	// battle triggers
	std::vector<Meeple*> losers;
	for (auto &chase : meeple_controller.chases) {
		float dist = glm::distance(chase->chaser->position(), chase->target->position());
		if (dist < 1.f) {
			chase->finished = true;
			losers.push_back(chase->target);
		}
	}

	for (int i = 0; i < losers.size(); i++) {
		if (losers[i] != meeple_controller.player.get()) {
			remove_meeple(losers[i]);
		}
	}
	
	debugger->update(camera);
}
	
void Campaign::display()
{
	scene->update(camera);
	scene->cull_frustum();
	scene->display();

	if (wireframe_worldmap) {
		board->display_wireframe(camera);
	} else {
		board->display(camera);
	}
	
	if (display_debug) {
		debugger->display_wireframe();
	}
}
	
void Campaign::remove_meeple(Meeple *meeple)
{
	physics.remove_object(meeple->trigger()->ghost_object());
	physics.remove_object(meeple->visibility()->ghost_object());

	auto duck_model = MediaManager::load_model("media/models/duck.glb");
	auto duck_object = scene->find_object(duck_model);
	duck_object->remove_transform(meeple->transform());

	debugger->remove_transform(meeple->trigger()->transform());
	debugger->remove_transform(meeple->visibility()->transform());

	meeple_controller.remove_meeple(meeple);
}
