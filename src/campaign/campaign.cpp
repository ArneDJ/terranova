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

enum class CampaignEntity : uint8_t {
	INVALID,
	LAND_SURFACE,
	WATER_SURFACE,
	MEEPLE,
	VILLAGE,
	TOWN
};

void Campaign::init(const gfx::ShaderGroup *shaders)
{
	debugger = std::make_unique<Debugger>(shaders->debug, shaders->debug, shaders->culling);

	labeler = std::make_unique<gfx::Labeler>("fonts/arial.ttf", 30, shaders->label);

	scene = std::make_unique<gfx::SceneGroup>(shaders->debug, shaders->culling);
	scene->set_scene_type(gfx::SceneType::DYNAMIC);
	
	board = std::make_unique<Board>(shaders->tilemap);

	// TODO pass this
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
		archive(seed);
		board->load(archive);
		archive(id_generator);
		archive(camera);
		archive(meeple_controller.meeples);
		archive(faction_controller);
		archive(settlement_controller);
		archive(player_data);
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
		archive(seed);
		board->save(archive);
		archive(id_generator);
		archive(camera);
		archive(meeple_controller.meeples);
		archive(faction_controller);
		archive(settlement_controller);
		archive(player_data);
	} else {
		LOG_F(ERROR, "Game saving error: could not open save file %s", filepath.c_str());
	}
}
	
void Campaign::generate(int seedling)
{
	seed = seedling;

	id_generator.reset();

	board->generate(seed);

	const auto &atlas = board->atlas();	
	const auto &tiles = atlas.tiles();	
	for (const auto &tile : tiles) {
		faction_controller.tile_owners[tile.index] = 0;
	}

	faction_controller.find_town_targets(atlas, 4 + 1);

	spawn_factions();

	player_data.meeple_id = id_generator.generate();
	meeple_controller.meeples[player_data.meeple_id] = std::make_unique<Meeple>();
	meeple_controller.player = meeple_controller.meeples[player_data.meeple_id].get();
	meeple_controller.player->set_id(player_data.meeple_id);
	meeple_controller.player->set_speed(5.f);

	meeple_controller.player->faction_id = player_data.faction_id;

	// position meeples at faction capitals
	for (auto &pair : meeple_controller.meeples) {
		auto &meeple = pair.second;
		auto faction_search = faction_controller.factions.find(meeple->faction_id);
		if (faction_search != faction_controller.factions.end()) {
			auto town_search = settlement_controller.towns.find(faction_search->second->capital_id);
			if (town_search != settlement_controller.towns.end()) {
				glm::vec2 center = board->tile_center(town_search->second->tile());
				meeple->teleport(center);
			}
		}
	}
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
	board->height_field()->object()->setUserIndex2(int(CampaignEntity::LAND_SURFACE));
	physics.add_object(board->height_field()->object(), group, mask);

	meeple_controller.player = meeple_controller.meeples[player_data.meeple_id].get();
	for (auto &meeple : meeple_controller.meeples) {
		meeple.second->sync();
		place_meeple(meeple.second.get());
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

	faction_controller.clear();

	settlement_controller.clear();
}

void Campaign::update(float delta)
{
	update_cheat_menu();

	physics.update_collision_only();

	update_camera(delta);

	if (player_mode == PlayerMode::ARMY_MOVEMENT) {
		if (util::InputManager::key_pressed(SDL_BUTTON_RIGHT)) {
			glm::vec3 ray = camera.ndc_to_ray(util::InputManager::abs_mouse_coords());
			set_player_movement(ray);
		}
	}
	
	if (player_mode == PlayerMode::TOWN_PLACEMENT) {
		glm::vec3 ray = camera.ndc_to_ray(util::InputManager::abs_mouse_coords());
		set_player_construction(ray);
	}

	meeple_controller.update(delta);

	update_meeple_target(meeple_controller.player);

	float offset = vertical_offset(meeple_controller.player->position());
	meeple_controller.player->set_vertical_offset(offset);

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
	trigger->ghost_object()->setUserIndex(meeple->id());
	trigger->ghost_object()->setUserIndex2(int(CampaignEntity::MEEPLE));
	physics.add_object(trigger->ghost_object(), COLLISION_GROUP_INTERACTION, meeple_mask);
	auto visibility = meeple->visibility();
	physics.add_object(visibility->ghost_object(), COLLISION_GROUP_VISIBILITY, COLLISION_GROUP_INTERACTION);

	auto duck_model = MediaManager::load_model(module.board_module.meeple);
	auto duck_object = scene->find_object(duck_model);
	duck_object->add_transform(meeple->transform());

	// debug trigger volumes
	debugger->add_sphere(trigger->form(), trigger->transform());
	debugger->add_sphere(visibility->form(), visibility->transform());

	// add label
	//glm::vec3 color = faction_controller.factions[meeple->faction()]->color();
	glm::vec3 color = glm::vec3(1.f);
	labeler->add_label(meeple->transform(), 0.2f, glm::vec3(0.f, 2.f, 0.f), "Army " + std::to_string(meeple->id()), color);
}
	
void Campaign::spawn_factions()
{
	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> dis(0.2f, 1.f);

	// first spawn player faction
	{
		std::unique_ptr<Faction> faction = std::make_unique<Faction>();
		auto id = id_generator.generate();
		faction->set_id(id);
		glm::vec3 color = { dis(gen), dis(gen), dis(gen) };
		faction->set_color(color);
		faction_controller.factions[id] = std::move(faction);

		player_data.faction_id = id;
	}

	for (int i = 0; i < 16; i++) {
		std::unique_ptr<Faction> faction = std::make_unique<Faction>();
		auto id = id_generator.generate();
		faction->set_id(id);
		glm::vec3 color = { dis(gen), dis(gen), dis(gen) };
		faction->set_color(color);
		faction_controller.factions[id] = std::move(faction);
	}

	// spawn their capitals
	std::queue<uint32_t> targets;
	for (const auto &target : faction_controller.town_targets) {
		targets.push(target);
	}

	const auto &atlas = board->atlas();	
	const auto &tiles = atlas.tiles();	

	for (const auto &pair : faction_controller.factions) {
		if (targets.empty()) {
			LOG_F(ERROR, "No more locations to spawn faction capital");
			break;
		}
		// assign a random tile as capital start
		uint32_t target = targets.front();
		targets.pop();
		auto &faction = pair.second;
		// place the town and assign it as faction capital
		uint32_t id = spawn_town(&tiles[target], faction->id());
		if (id) {
			faction->capital_id = id;
			Town *town = settlement_controller.towns[id].get();
			place_town(town);
			spawn_county(town);
		}
	}
}
	
// returns the id of the town if it could be added on an unoccupied tile
uint32_t Campaign::spawn_town(const Tile *tile, uint32_t faction)
{
	if (tile->relief != ReliefType::LOWLAND && tile->relief != ReliefType::HILLS) {
		return 0;
	}

	auto search = faction_controller.tile_owners.find(tile->index);
	if (search == faction_controller.tile_owners.end() || search->second == 0) {
		std::unique_ptr<Town> town = std::make_unique<Town>();
		auto id = id_generator.generate();
		town->set_id(id);
		town->set_faction(faction);
		town->set_tile(tile->index);

		settlement_controller.towns[id] = std::move(town);

		faction_controller.tile_owners[tile->index] = faction;

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
				bool passable = tile->relief == ReliefType::LOWLAND || tile->relief == ReliefType::HILLS;
				if (passable && (faction_controller.tile_owners[tile->index] == 0)) {
					depth[tile->index] = layer;
					faction_controller.tile_owners[tile->index] = faction;
					nodes.push(tile->index);
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

	const int mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_VISIBILITY | COLLISION_GROUP_RAY;

	auto trigger = town->trigger();
	trigger->ghost_object()->setUserIndex(town->id());
	trigger->ghost_object()->setUserIndex2(int(CampaignEntity::TOWN));
	physics.add_object(trigger->ghost_object(), COLLISION_GROUP_TOWN, mask);

	// debug collision
	debugger->add_sphere(trigger->form(), trigger->transform());

	// add label
	glm::vec3 color = faction_controller.factions[town->faction()]->color();
	labeler->add_label(town->transform(), 1.f, glm::vec3(0.f, 3.f, 0.f), "Town " + std::to_string(town->id()), color);
}
	
void Campaign::reset_camera()
{
	camera.position = meeple_controller.player->transform()->position + glm::vec3(0.f, 10.f, -10.f);
	camera.target(meeple_controller.player->transform()->position);
}
	
void Campaign::set_meeple_target(Meeple *meeple, uint32_t target_id, uint8_t target_type)
{
	meeple->target_type = 0;
	meeple->target_id = 0;

	CampaignEntity entity_type = CampaignEntity(target_type);
	if (entity_type == CampaignEntity::TOWN) {
		auto search = settlement_controller.towns.find(target_id);
		if (search != settlement_controller.towns.end()) {
			meeple->target_type = target_type;
			meeple->target_id = target_id;
		}
	} else if (entity_type == CampaignEntity::LAND_SURFACE) {
		meeple->target_type = target_type;
	}
}

void Campaign::update_meeple_target(Meeple *meeple)
{
	CampaignEntity entity_type = CampaignEntity(meeple->target_type);
	if (entity_type == CampaignEntity::TOWN) {
		auto search = settlement_controller.towns.find(meeple->target_id);
		if (search != settlement_controller.towns.end()) {
			float distance = glm::distance(meeple->position(), search->second->map_position());
			if (distance < 1.F) {
				meeple->clear_target();
				// add town event
				if (meeple->id() == player_data.meeple_id) {
					battle_data.tile = search->second->tile();
					battle_data.town_size = 2;
					state = CampaignState::BATTLE_REQUEST;
				}
			}
		} else {
			// town not found clear target
			meeple->clear_target();
		}
	}
}
	
void Campaign::update_cheat_menu()
{
	ImGui::Begin("Cheat Menu");
	ImGui::SetWindowSize(ImVec2(300, 200));
	if (ImGui::Button("Add Town")) {
		if (player_mode != PlayerMode::TOWN_PLACEMENT) {
			player_mode = PlayerMode::TOWN_PLACEMENT;
		} else {
			player_mode = PlayerMode::ARMY_MOVEMENT;
		}
	}
	ImGui::End();
}
	
void Campaign::update_camera(float delta)
{
	float modifier = 10.f * delta;
	float speed = 5.f * modifier;
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
	
void Campaign::set_player_movement(const glm::vec3 &ray)
{
	auto result = physics.cast_ray(camera.position, camera.position + (1000.f * ray), COLLISION_GROUP_HEIGHTMAP | COLLISION_GROUP_TOWN);
	if (result.hit) {
		if (result.object) {
			set_meeple_target(meeple_controller.player, result.object->getUserIndex(), result.object->getUserIndex2());
			// set initial path
			glm::vec2 hitpoint = glm::vec2(result.point.x, result.point.z);
			std::list<glm::vec2> nodes;
			
			board->find_path(meeple_controller.player->position(), hitpoint, nodes);
			meeple_controller.player->set_path(nodes);
		}
		if (meeple_controller.player->target_type == uint8_t(CampaignEntity::LAND_SURFACE)) {
			marker.teleport(result.point);
		}
	}
}
	
void Campaign::set_player_construction(const glm::vec3 &ray)
{
	auto result = physics.cast_ray(camera.position, camera.position + (1000.f * ray), COLLISION_GROUP_HEIGHTMAP);
	const Tile *tile = board->atlas().tile_at(glm::vec2(result.point.x, result.point.z));
	if (tile) {
		glm::vec2 center = board->tile_center(tile->index);
		float offset = vertical_offset(center);
		marker.teleport(glm::vec3(center.x, offset, center.y));
		if (util::InputManager::key_pressed(SDL_BUTTON_LEFT)) {
			uint32_t id = spawn_town(tile, player_data.faction_id);
			if (id) {
				Town *town = settlement_controller.towns[id].get();
				place_town(town);
				spawn_county(town);
				// change mode
				player_mode = PlayerMode::ARMY_MOVEMENT;
			}
		}
	}
}
