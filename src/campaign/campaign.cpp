#include <iostream>
#include <chrono>
#include <unordered_map>
#include <list>
#include <memory>
#include <queue>
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
#include "../extern/cereal/archives/json.hpp"

#include "../extern/freetypegl/freetype-gl.h"

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
#include "../graphics/texture.h"
#include "../graphics/scene.h"
#include "../graphics/label.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"
#include "../physics/trigger.h"

#include "../debugger.h"
#include "../media.h"
#include "../module.h"

#include "campaign.h"

enum CampaignCollisionGroup {
	COLLISION_GROUP_NONE = 0,
	COLLISION_GROUP_RAY = 1 << 0,
	COLLISION_GROUP_INTERACTION = 1 << 1,
	COLLISION_GROUP_TOWN = 1 << 2,
	COLLISION_GROUP_VISIBILITY = 1 << 3,
	COLLISION_GROUP_HEIGHTMAP = 1 << 4
};
	
void Campaign::init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tilemap)
{
	debugger = std::make_unique<Debugger>(visual, visual, culling);

	labeler = std::make_unique<gfx::Labeler>("fonts/arial.ttf", 30);

	scene = std::make_unique<gfx::SceneGroup>(visual, culling);
	scene->set_scene_type(gfx::SceneType::DYNAMIC);
	
	board = std::make_unique<Board>(tilemap);

	meeple_controller.player = std::make_unique<Meeple>();

	// load module
	std::ifstream stream("modules/native/board.json");
	if (stream.is_open()) {
		cereal::JSONInputArchive archive(stream);
		archive(module.board_module);
	}
}
	
void Campaign::load(const std::string &filepath)
{
	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(name);
		board->load(archive);
		archive(id_generator);
		archive(camera);
		archive(meeple_controller.player);
		archive(meeple_controller.meeples);
		archive(faction_controller);
		archive(settlement_controller);
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
		archive(id_generator);
		archive(camera);
		archive(meeple_controller.player);
		archive(meeple_controller.meeples);
		archive(faction_controller);
		archive(settlement_controller);
	} else {
		LOG_F(ERROR, "Game saving error: could not open save file %s", filepath.c_str());
	}
}
	
void Campaign::generate(int seed)
{
	id_generator.reset();

	board->generate(seed);

	const auto &atlas = board->atlas();	
	const auto &tiles = atlas.tiles();	
	for (const auto &tile : tiles) {
		faction_controller.tile_owners[tile.index] = 0;
	}

	// spawn factions
	spawn_factions(seed);

	meeple_controller.player = std::make_unique<Meeple>();
	meeple_controller.player->teleport(glm::vec2(512.f));
	meeple_controller.player->set_speed(10.f);
	meeple_controller.player->set_name("Player Army");
}
	
void Campaign::prepare()
{
	board->reload();

	auto cone_model = MediaManager::load_model(module.board_module.marker);
	auto cone_object = scene->find_object(cone_model);
	cone_object->add_transform(marker.transform());

	// add physical objects
	int group = COLLISION_GROUP_HEIGHTMAP;
	int mask = COLLISION_GROUP_RAY;
	physics.add_object(board->height_field()->object(), group, mask);

	meeple_controller.player->sync();
	place_meeple(meeple_controller.player.get());

	for (auto &meeple : meeple_controller.meeples) {
		meeple->sync();
		place_meeple(meeple.get());
	}

	// place towns
	for (auto &town : settlement_controller.towns) {
		place_town(town.second.get());
	}

	// paint county tiles
	for (const auto &county : settlement_controller.counties) {
		glm::vec3 color = faction_controller.factions[county.second->faction()]->color();
		for (const auto &tile : county.second->tiles()) {
			board->add_paint_job(tile, color);
		}
	}

	board->update();
}

void Campaign::clear()
{
	// first clear the model instances
	scene->clear_instances();

	labeler->clear();

	debugger->clear();

	// clear physical objects
	physics.clear_objects();

	// clear entities
	meeple_controller.clear();
	meeple_controller.player = std::make_unique<Meeple>();

	faction_controller.clear();

	settlement_controller.clear();
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

	float offset = vertical_offset(meeple_controller.player->position());
	meeple_controller.player->set_vertical_offset(offset);

	if (util::InputManager::key_pressed(SDLK_b)) { 
		const Tile *tile = board->atlas().tile_at(meeple_controller.player->position());
		if (tile) {
			uint32_t id = spawn_town(tile->index, 1);
			if (id) {
				Town *town = settlement_controller.towns[id].get();
				place_town(town);
				spawn_county(town);
			}
		}
	}

	debugger->update(camera);

	// campaign map paint jobs
	board->update();
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

	labeler->display(camera);
}
	
float Campaign::vertical_offset(const glm::vec2 &position)
{
	glm::vec3 origin = { position.x, 2.F * board->SCALE.y, position.y };
	glm::vec3 end = { position.x, 0.F, position.y };

	auto result = physics.cast_ray(origin, end, COLLISION_GROUP_HEIGHTMAP);
	
	return result.point.y;
}
	
void Campaign::place_meeple(Meeple *meeple)
{
	float offset = vertical_offset(meeple->position());
	meeple->set_vertical_offset(offset);

	int meeple_mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_VISIBILITY | COLLISION_GROUP_RAY | COLLISION_GROUP_TOWN;

	auto trigger = meeple->trigger();
	physics.add_object(trigger->ghost_object(), COLLISION_GROUP_INTERACTION, meeple_mask);
	auto visibility = meeple->visibility();
	physics.add_object(visibility->ghost_object(), COLLISION_GROUP_VISIBILITY, COLLISION_GROUP_INTERACTION);

	auto duck_model = MediaManager::load_model(module.board_module.meeple);
	auto duck_object = scene->find_object(duck_model);
	duck_object->add_transform(meeple->transform());

	// debug trigger volumes
	debugger->add_sphere(trigger->form(), trigger->transform());
	debugger->add_sphere(visibility->form(), visibility->transform());
}
	
void Campaign::spawn_factions(int seed)
{
	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> dis(0.2f, 1.f);

	for (int i = 0; i < 10; i++) {
		std::unique_ptr<Faction> faction = std::make_unique<Faction>();
		auto id = id_generator.generate();
		faction->set_id(id);
		glm::vec3 color = { dis(gen), dis(gen), dis(gen) };
		faction->set_color(color);
		faction_controller.factions[id] = std::move(faction);
	}
}
	
// returns the id of the town if it could be added on an unoccupied tile
uint32_t Campaign::spawn_town(uint32_t tile, uint32_t faction)
{
	auto search = faction_controller.tile_owners.find(tile);
	if (search == faction_controller.tile_owners.end() || search->second == 0) {
		std::unique_ptr<Town> town = std::make_unique<Town>();
		auto id = id_generator.generate();
		town->set_id(id);
		town->set_faction(faction);
		town->set_tile(tile);

		auto trigger = town->trigger();
		trigger->ghost_object()->setUserIndex(id);

		settlement_controller.towns[id] = std::move(town);

		faction_controller.tile_owners[tile] = faction;

		return id;
	}

	return 0;
}

void Campaign::spawn_county(Town *town)
{
	int radius = 4;

	const auto faction = town->faction();

	const auto id = id_generator.generate();

	std::unique_ptr<County> county = std::make_unique<County>();
	county->set_id(id);
	county->set_faction(faction);
	county->set_town(town->id());

	town->set_county(id);

	// breadth first search
	const auto &atlas = board->atlas();	
	const auto &cells = atlas.graph().cells;	
	const auto &tiles = atlas.tiles();	

	std::unordered_map<uint32_t, uint32_t> depth;
	std::queue<uint32_t> nodes;

	nodes.push(town->tile());
	depth[town->tile()] = 0;
	faction_controller.tile_owners[town->tile()] = faction;

	while (!nodes.empty()) {
		auto node = nodes.front();
		nodes.pop();
		county->add_tile(node);
		uint32_t layer = depth[node] + 1;
		if (layer < radius) {
			const auto &cell = cells[node];
			for (const auto &neighbor : cell.neighbors) {
				const Tile *tile = &tiles[neighbor->index];
				bool valid_relief = tile->relief == ReliefType::LOWLAND || tile->relief == ReliefType::HILLS;
				if (valid_relief) {
					if (faction_controller.tile_owners[tile->index] == 0) {
						depth[tile->index] = layer;
						faction_controller.tile_owners[tile->index] = faction;
						nodes.push(tile->index);
					}
				}
			}
		}
	}

	// add tile paint jobs
	glm::vec3 color = faction_controller.factions[faction]->color();
	for (const auto &tile : county->tiles()) {
		board->add_paint_job(tile, color);
	}

	settlement_controller.counties[id] = std::move(county);
}

// place the town entity on the campaign map
void Campaign::place_town(Town *town)
{
	glm::vec2 center = board->tile_center(town->tile());
	float offset = vertical_offset(center);

	town->set_position(glm::vec3(center.x, offset, center.y));

	auto cylinder_model = MediaManager::load_model(module.board_module.town);
	auto cylinder_object = scene->find_object(cylinder_model);
	cylinder_object->add_transform(town->transform());

	// debug collision
	auto trigger = town->trigger();
	debugger->add_sphere(trigger->form(), trigger->transform());

	const int mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_VISIBILITY | COLLISION_GROUP_RAY;

	physics.add_object(trigger->ghost_object(), COLLISION_GROUP_TOWN, mask);

	// add label
	glm::vec3 color = faction_controller.factions[town->faction()]->color();
	labeler->add_label(town->transform(), glm::vec3(0.f, 3.f, 0.f), "Town " + std::to_string(town->id()), color);
}
	
void Campaign::reset_camera()
{
	camera.position = meeple_controller.player->transform()->position + glm::vec3(0.f, 10.f, -10.f);
	camera.target(meeple_controller.player->transform()->position);
}
