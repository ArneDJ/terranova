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
#include "../util/animation.h"
#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../geometry/voronoi.h"
#include "../util/image.h"
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

static const float GAME_TIME_SLICE = 1.f; // in seconds time needed to update game tick

// initializes the campaign
void Campaign::init(const gfx::ShaderGroup *shaders)
{
	debugger = std::make_unique<Debugger>(shaders->debug);

	labeler = std::make_unique<gfx::Labeler>("fonts/diablo.ttf", 30, shaders->label);

	board = std::make_unique<Board>(shaders->tilemap);
	
	object_shader = shaders->debug;
	meeple_shader = shaders->creature;
}
	
// load campaign bueprints from the module
void Campaign::load_blueprints(const Module &module)
{
	town_blueprint.model = MediaManager::load_model(module.board_module.town);
	con_marker.model = MediaManager::load_model(module.board_module.town);

	army_blueprint.model = MediaManager::load_model(module.board_module.meeple);
	// load skeletons and animations for miniatures
	army_blueprint.anim_set = std::make_unique<util::AnimationSet>();
	army_blueprint.anim_set->skeleton = MediaManager::load_skeleton(module.board_module.meeple_skeleton);
	army_blueprint.anim_set->animations[MA_IDLE] = MediaManager::load_animation(module.board_module.meeple_anim_idle);
	army_blueprint.anim_set->animations[MA_RUN] = MediaManager::load_animation(module.board_module.meeple_anim_run);
	army_blueprint.anim_set->find_max_tracks();
}
	
// loads a campaign save game
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
		archive(game_ticks);
		archive(faction_ticks);
	} else {
		LOG_F(ERROR, "Game loading error: could not open save file %s", filepath.c_str());
	}
}

// saves a campaign to file
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
		archive(game_ticks);
		archive(faction_ticks);
	} else {
		LOG_F(ERROR, "Game saving error: could not open save file %s", filepath.c_str());
	}
}
	
// generates a new campaign world based on a seed
void Campaign::generate(int seedling)
{
	// reset game ticks
	game_ticks = 0;
	faction_ticks = 0;
	
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
	meeple_controller.player->id = player_data.meeple_id;
	meeple_controller.player->set_speed(4.f);

	meeple_controller.player->faction_id = player_data.faction_id;

	// position meeples at faction capitals
	for (auto &mapping : meeple_controller.meeples) {
		auto &meeple = mapping.second;
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
	
// prepares a new or loaded campaign
void Campaign::prepare()
{
	board->reload();

	// add physical objects
	int group = COLLISION_GROUP_HEIGHTMAP;
	int mask = COLLISION_GROUP_RAY;
	board->height_field()->object()->setUserIndex2(int(CampaignEntity::LAND_SURFACE));
	physics.add_object(board->height_field()->object(), group, mask);

	// place towns
	for (auto &town : settlement_controller.towns) {
		place_town(town.second.get());
	}

	// paint fiefdom tiles
	for (const auto &fiefdom : settlement_controller.fiefdoms) {
		glm::vec3 color = faction_controller.factions[fiefdom.second->faction()]->color();
		for (const auto &tile : fiefdom.second->tiles()) {
			board->paint_tile(tile, color);
		}
	}

	const auto &atlas = board->atlas();	
	const auto &graph = atlas.graph();	
	const auto &tiles = atlas.tiles();	
	const auto &borders = atlas.borders();	
	for (const auto &tile : tiles) {
		glm::vec3 color = tile.relief == ReliefType::SEABED ? glm::vec3(1.f, 0.f, 1.f) : glm::vec3(0.f, 0.f, 0.f);
		if (tile.flags & TILE_FLAG_COAST) {
			//board->paint_tile(tile.index, color);
		}
		const auto &cell = graph.cells[tile.index];
		for (const auto &edge : cell.edges) {
			const auto &border = borders[edge->index];
			if (border.flags & BORDER_FLAG_RIVER) {
				board->paint_border(tile.index, border.index, color);
			}
		}
	}

	board->update();

	meeple_controller.player = meeple_controller.meeples[player_data.meeple_id].get();
	for (auto &mapping : meeple_controller.meeples) {
		auto &meeple = mapping.second;
		// set animations TODO should be done earlier during object construction
		meeple->set_animation(army_blueprint.anim_set.get());
		meeple->sync();
		place_meeple(meeple.get());
		meeple->update_animation(0.01f); // initial pose
	}
	
	debugger->add_navmesh(board->navigation().navmesh());

	// game is paused at the start
	state = CampaignState::PAUSED;
	// marker is not present at start
	board->hide_marker();
}

// clears all the campaign data
void Campaign::clear()
{
	labeler->clear();

	debugger->clear();

	// clear physical objects
	physics.clear_objects();

	// clear entities
	meeple_controller.clear();

	faction_controller.clear();

	settlement_controller.clear();
}

// the campaign game update in a frame
void Campaign::update(float delta)
{
	// check if player wants to pause the game
	if (util::InputManager::key_pressed(SDLK_p)) {
		if (state == CampaignState::RUNNING) {
			state = CampaignState::PAUSED;
		} else if (state == CampaignState::PAUSED) {
			state = CampaignState::RUNNING;
		}
	}

	// accumulate time for game tick if not paused
	if (state == CampaignState::RUNNING) {
		hourglass_sand += delta;
		if (hourglass_sand > GAME_TIME_SLICE) {
			float integral = floorf(hourglass_sand);
			game_ticks += integral;
			hourglass_sand -= integral; // reset plus fractional part
		}
	}

	update_debug_menu();

	update_camera(delta);

	physics.update_collision_only();

	if (player_mode == PlayerMode::ARMY_MOVEMENT) {
		if (util::InputManager::key_pressed(SDL_BUTTON_RIGHT)) {
			glm::vec3 ray = camera.ndc_to_ray(util::InputManager::abs_mouse_coords());
			set_player_movement(ray);
		}
	}
	
	if (player_mode == PlayerMode::TOWN_PLACEMENT) {
		glm::vec3 ray = camera.ndc_to_ray(util::InputManager::abs_mouse_coords());
		set_player_construction(ray);
	} else {
		con_marker.visible = false;
	}

	// if the game isn't paused update gameplay
	if (state == CampaignState::RUNNING) {
		update_factions();
		meeple_controller.update(delta);
		update_meeple_target(meeple_controller.player);
	}

	// if player reached the target hide marker
	if (meeple_controller.player->target_type == 0) {
		board->hide_marker();
	}

	float offset = vertical_offset(meeple_controller.player->position());
	meeple_controller.player->set_vertical_offset(offset);

	// campaign map paint jobs
	board->update();
}
	
// renders whatever happens in a campaign
void Campaign::display()
{
	// display the construction markers
	object_shader->use();
	object_shader->uniform_mat4("CAMERA_VP", camera.VP);
	if (con_marker.visible) {
		object_shader->uniform_mat4("MODEL", con_marker.transform.to_matrix());
		con_marker.model->display();
	}

	// display town entities
	for (auto &mapping : settlement_controller.towns) {
		auto &town = mapping.second;
		object_shader->uniform_mat4("MODEL", town->transform()->to_matrix());
		town->model()->display();
	}

	// display army entities
	meeple_shader->use();
	meeple_shader->uniform_mat4("CAMERA_VP", camera.VP);
	for (auto &mapping : meeple_controller.meeples) {
		auto &meeple = mapping.second;
		meeple_shader->uniform_mat4("MODEL", meeple->transform.to_matrix());
		//meeple->model->display();
		meeple->display();
	}

	if (wireframe_worldmap) {
		board->display_wireframe(camera);
	} else {
		board->display(camera);
	}
	
	if (display_debug) {
		//debugger->display_wireframe();
		debugger->display_navmeshes();
	}

	labeler->display(camera);
}
	
// returns the vertical offset of the campaign heightmap at map coordinates
float Campaign::vertical_offset(const glm::vec2 &position)
{
	glm::vec3 origin = { position.x, 2.F * board->SCALE.y, position.y };
	glm::vec3 end = { position.x, 0.F, position.y };

	auto result = physics.cast_ray(origin, end, COLLISION_GROUP_HEIGHTMAP);
	
	return result.point.y;
}
	
// places a meeple entity on the campaign map
void Campaign::place_meeple(Meeple *meeple)
{
	float offset = vertical_offset(meeple->position());
	meeple->set_vertical_offset(offset);

	int meeple_mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_VISIBILITY | COLLISION_GROUP_RAY | COLLISION_GROUP_TOWN;

	auto trigger = meeple->trigger();
	trigger->ghost_object()->setUserIndex(meeple->id);
	trigger->ghost_object()->setUserIndex2(int(CampaignEntity::MEEPLE));
	physics.add_object(trigger->ghost_object(), COLLISION_GROUP_INTERACTION, meeple_mask);
	auto visibility = meeple->visibility();
	physics.add_object(visibility->ghost_object(), COLLISION_GROUP_VISIBILITY, COLLISION_GROUP_INTERACTION);

	meeple->model = army_blueprint.model;

	// add label
	glm::vec3 color = faction_controller.factions[meeple->faction_id]->color();
	//glm::vec3 color = glm::vec3(1.f);
	labeler->add_label(&meeple->transform, 0.2f, glm::vec3(0.f, 2.f, 0.f), "Army " + std::to_string(meeple->id), color);
}
	
// spawns a group of new factions
// this is used when a new campaign game is created
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

	for (int i = 0; i < 24; i++) {
		std::unique_ptr<Faction> faction = std::make_unique<Faction>();
		auto id = id_generator.generate();
		faction->set_id(id);
		glm::vec3 color = { dis(gen), dis(gen), dis(gen) };
		faction->set_color(color);
		faction_controller.factions[id] = std::move(faction);
	}

	const auto &atlas = board->atlas();	
	const auto &tiles = atlas.tiles();	

	// spawn their capitals
	std::queue<uint32_t> targets;
	for (const auto &target : faction_controller.town_targets) {
		// disregard island spawns
		const auto &tile = tiles[target];
		if (!(tile.flags & TILE_FLAG_ISLAND)) {
			targets.push(target);
		}
	}

	for (const auto &mapping : faction_controller.factions) {
		if (targets.empty()) {
			LOG_F(ERROR, "No more locations to spawn faction capital");
			break;
		}
		// assign a random tile as capital start
		uint32_t target = targets.front();
		targets.pop();
		auto &faction = mapping.second;
		// place the town and assign it as faction capital
		uint32_t id = spawn_town(&tiles[target], faction.get());
		if (id) {
			faction->capital_id = id;
			Town *town = settlement_controller.towns[id].get();
			spawn_fiefdom(town);
		}
	}
}
	
// returns the id of the town if it could be added on an unoccupied tile
uint32_t Campaign::spawn_town(const Tile *tile, Faction *faction)
{
	if (tile->relief != ReliefType::LOWLAND && tile->relief != ReliefType::HILLS) {
		return 0;
	}

	NameGen::Generator namegen(MIDDLE_EARTH);

	auto search = faction_controller.tile_owners.find(tile->index);
	if (search == faction_controller.tile_owners.end() || search->second == 0) {
		std::unique_ptr<Town> town = std::make_unique<Town>();
		auto id = id_generator.generate();
		town->set_id(id);
		town->set_faction(faction->id());
		town->set_tile(tile->index);

		if (tile->flags & TILE_FLAG_RIVER) {
			town->set_size(3);
		} else {
			town->set_size(1);
		}

		// give town a name
		town->name = namegen.toString();

		settlement_controller.towns[id] = std::move(town);

		faction_controller.tile_owners[tile->index] = faction->id();

		// add town to faction
		faction->towns.push_back(id);

		return id;
	}

	return 0;
}

// spawns a new valid town entity
void Campaign::spawn_fiefdom(Town *town)
{
	int radius = 4;

	const auto faction = town->faction();

	const auto id = id_generator.generate();

	std::unique_ptr<Fiefdom> fiefdom = std::make_unique<Fiefdom>();
	fiefdom->set_id(id);
	fiefdom->set_faction(faction);
	fiefdom->set_town(town->id());

	town->set_fiefdom(id);

	// breadth first search
	const auto &atlas = board->atlas();	
	const auto &cells = atlas.graph().cells;	
	const auto &tiles = atlas.tiles();	
	const auto &borders = atlas.borders();	

	std::unordered_map<uint32_t, uint32_t> depth;
	std::queue<uint32_t> nodes;

	nodes.push(town->tile());
	depth[town->tile()] = 0;
	faction_controller.tile_owners[town->tile()] = faction;

	while (!nodes.empty()) {
		auto node = nodes.front();
		nodes.pop();
		fiefdom->add_tile(node);
		uint32_t layer = depth[node] + 1;
		if (layer < radius) {
			const auto &cell = cells[node];
			for (const auto &edge : cell.edges) {
				const auto &border = borders[edge->index];
				// if no river is between them
				if (!(border.flags & BORDER_FLAG_RIVER) && !(border.flags & BORDER_FLAG_FRONTIER)) {
					auto neighbor_index = edge->left_cell->index == node ? edge->right_cell->index : edge->left_cell->index;
					const Tile *neighbor = &tiles[neighbor_index];
					if (walkable_tile(neighbor) && (faction_controller.tile_owners[neighbor->index] == 0)) {
						depth[neighbor->index] = layer;
						faction_controller.tile_owners[neighbor->index] = faction;
						nodes.push(neighbor->index);
					}
				}
			}
		}
	}

	// add tile paint jobs
	glm::vec3 color = faction_controller.factions[faction]->color();
	for (const auto &tile : fiefdom->tiles()) {
		board->paint_tile(tile, color);
	}

	settlement_controller.fiefdoms[id] = std::move(fiefdom);
}

// place the town entity on the campaign map
void Campaign::place_town(Town *town)
{
	glm::vec2 center = board->tile_center(town->tile());
	float offset = vertical_offset(center);

	town->set_position(glm::vec3(center.x, offset, center.y));

	town->set_model(town_blueprint.model); // TODO needs to be done at spawn

	const int mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_VISIBILITY | COLLISION_GROUP_RAY;

	auto trigger = town->trigger();
	trigger->ghost_object()->setUserIndex(town->id());
	trigger->ghost_object()->setUserIndex2(int(CampaignEntity::TOWN));
	physics.add_object(trigger->ghost_object(), COLLISION_GROUP_TOWN, mask);

	// add label
	glm::vec3 color = faction_controller.factions[town->faction()]->color();
	labeler->add_label(town->transform(), 1.f, glm::vec3(0.f, 3.f, 0.f), town->name, color);
}
	
// teleports the camera to the player position
void Campaign::reset_camera()
{
	camera.position = meeple_controller.player->transform.position + glm::vec3(0.f, 10.f, -10.f);
	camera.target(meeple_controller.player->transform.position);
}
	
// changes the target entity of a meeple
// checks if it is a valid entity
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

BoardMarker Campaign::marker_data(const glm::vec2 &hitpoint, uint32_t target_id, uint8_t target_type)
{
	BoardMarker marker = { hitpoint, glm::vec3(0.f), 2.f, 1.f };

	// if target is an entity change marker position to target center

	CampaignEntity type = CampaignEntity(target_type);
	if (type == CampaignEntity::LAND_SURFACE) {
		marker.color = glm::vec3(0.8f, 0.8f, 1.f);
	} else if (type == CampaignEntity::TOWN) {
		auto search = settlement_controller.towns.find(target_id);
		if (search != settlement_controller.towns.end()) {
			auto &town = search->second;
			marker.position = town->map_position();
			marker.radius = 5.f;
			if (town->faction() == player_data.faction_id) {
				marker.color = glm::vec3(0.f, 1.f, 0.f);
			} else {
				marker.color = glm::vec3(1.f, 1.f, 0.f);
			}
		}
	} else if (type == CampaignEntity::MEEPLE) {
	}

	return marker;
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
				if (meeple->id == player_data.meeple_id) {
					battle_data.tile = search->second->tile();
					battle_data.town_size = search->second->size();
					state = CampaignState::BATTLE_REQUEST;
				}
			}
		} else {
			// town not found clear target
			meeple->clear_target();
		}
	}
}
	
void Campaign::update_debug_menu()
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
	if (ImGui::Button("Visit Tile")) {
		visit_current_tile();
	}
	ImGui::End();

	// tell player if game is paused
	if (state == CampaignState::PAUSED) {
		ImGui::Begin("Game is paused");
		ImGui::SetWindowSize(ImVec2(150, 100));
		ImGui::End();
	}

	// game stats
	ImGui::Begin("Game stats");
	ImGui::SetWindowSize(ImVec2(300, 200));
	ImGui::Text("Faction gold: %d", faction_controller.factions[player_data.faction_id]->gold());
	ImGui::Text("Total Faction towns: %d", faction_controller.factions[player_data.faction_id]->towns.size());
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

	// collide with map
	float offset = vertical_offset(glm::vec2(camera.position.x, camera.position.z)) + 5.f;
	if (camera.position.y < offset) {
		camera.position.y = offset;
	}

	camera.update_viewing();
}
	
void Campaign::set_player_movement(const glm::vec3 &ray)
{
	auto result = physics.cast_ray(camera.position, camera.position + (1000.f * ray), COLLISION_GROUP_HEIGHTMAP | COLLISION_GROUP_TOWN);
	if (result.hit && result.object) {
		set_meeple_target(meeple_controller.player, result.object->getUserIndex(), result.object->getUserIndex2());
		// find initial path
		glm::vec2 hitpoint = glm::vec2(result.point.x, result.point.z);
		auto marker = marker_data(hitpoint, result.object->getUserIndex(), result.object->getUserIndex2());
		std::list<glm::vec2> nodes;
		board->find_path(meeple_controller.player->position(), marker.position, nodes);
		// update visual marker
		// marker color is based on entity type
		if (nodes.size()) {
			marker.position = nodes.back();
			board->set_marker(marker);
			meeple_controller.player->set_path(nodes);

			// found a path so unpause
			// if in pause mode unpause game
			if (state == CampaignState::PAUSED) {
				state = CampaignState::RUNNING;
			}
		}
	}
}
	
void Campaign::set_player_construction(const glm::vec3 &ray)
{
	auto result = physics.cast_ray(camera.position, camera.position + (1000.f * ray), COLLISION_GROUP_HEIGHTMAP);
	const Tile *tile = board->atlas().tile_at(glm::vec2(result.point.x, result.point.z));
	const int town_cost = 100;

	if (tile) {
		glm::vec2 center = board->tile_center(tile->index);
		float offset = vertical_offset(center);
		con_marker.transform.position = glm::vec3(center.x, offset, center.y);
		con_marker.visible = true;
		if (util::InputManager::key_pressed(SDL_BUTTON_LEFT)) {
			if (faction_controller.factions[player_data.faction_id]->gold() >= town_cost) {
				uint32_t id = spawn_town(tile, faction_controller.factions[player_data.faction_id].get());
				if (id) {
					Town *town = settlement_controller.towns[id].get();
					place_town(town);
					spawn_fiefdom(town);
					// change mode
					player_mode = PlayerMode::ARMY_MOVEMENT;
					// town costs money
					faction_controller.factions[player_data.faction_id]->add_gold(-town_cost);
				}
			}
		}
	}
}

void Campaign::transfer_town(Town *town, uint32_t faction)
{
	auto &fiefdom = settlement_controller.fiefdoms[town->fiefdom()];
	fiefdom->set_faction(faction);

	// change fiefdom tiles faction
	glm::vec3 color = faction_controller.factions[faction]->color();
	for (const auto &tile : fiefdom->tiles()) {
		faction_controller.tile_owners[tile] = faction;
		board->paint_tile(tile, color);
	}

	// transfer capital
	town->set_faction(faction);
	labeler->change_text_color(town->transform(), color);
}
	
// only used in cheat mode
void Campaign::visit_current_tile()
{
	const auto &tile = board->tile_at(meeple_controller.player->position());

	if (tile) {
		battle_data.tile = tile->index;
		battle_data.town_size = 0;
		state = CampaignState::BATTLE_REQUEST;
	}
}
	
// updates faction treasuries based on their holdings
void Campaign::update_faction_taxes()
{
	for (const auto &mapping : faction_controller.factions) {
		auto &faction = mapping.second;
		auto profit = 2 * faction->towns.size();
		faction->add_gold(profit);
	}
}
	
// update faction behavior
void Campaign::update_factions()
{
	// update faction gold after an elapsed game time period
	// in game this means every month
	if (game_ticks % 60 == 0) {
		if (game_ticks > faction_ticks) {
			update_faction_taxes();
			faction_ticks = game_ticks;
		}
	}

	// factions will look for new tiles to settle
	for (const auto &mapping : faction_controller.factions) {
		auto &faction = mapping.second;
		// if the faction is not already expanding 
		if (!faction->expanding) {
			// has enough gold it will look for a new tile to settle
			// is not overextended by having too many towns too
			if (faction->towns.size() < 32 && faction->gold() >= 100) {
				faction->expanding = true;
				// add a request to expand
				faction_controller.add_expand_request(faction->id());
			}
		}
	}

	// fill out expansion request 
	auto faction_id = faction_controller.top_request();
	if (faction_id) {
		Faction *faction = faction_controller.factions[faction_id].get();
		auto town_search = settlement_controller.towns.find(faction->capital_id);
		if (town_search != settlement_controller.towns.end()) {
			uint32_t capital_tile = town_search->second->tile();
			if (faction->gold() >= 100) {
				const auto &atlas = board->atlas();	
				uint32_t tile = faction_controller.find_closest_town_target(atlas, faction, capital_tile);
				if (tile) {
					uint32_t id = spawn_town(&atlas.tiles()[tile], faction);
					if (id) {
						Town *town = settlement_controller.towns[id].get();
						place_town(town);
						spawn_fiefdom(town);
						// town costs money
						faction->add_gold(-100);
						// expansion done so change mode
						faction->expanding = false;
					}
				}
			}
		}
	}
}
