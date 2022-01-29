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

#include "../extern/poisson/PoissonGenerator.h"

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
#include "../console.h"

#include "campaign.h"

enum CampaignCollisionGroup {
	COLLISION_GROUP_NONE = 0,
	COLLISION_GROUP_RAY = 1 << 0,
	COLLISION_GROUP_INTERACTION = 1 << 1,
	COLLISION_GROUP_VISIBILITY = 1 << 2,
	COLLISION_GROUP_HEIGHTMAP = 1 << 3
};

enum class CampaignEntityType : uint8_t {
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

	labeler = std::make_unique<gfx::LabelFont>("fonts/exocet.ttf", 30);

	board = std::make_unique<Board>(shaders->tilemap, shaders->blur);
	
	object_shader = shaders->debug;
	meeple_shader = shaders->creature;
	font_shader = shaders->font;
	label_shader = shaders->label;
}
	
// load campaign bueprints from the module
void Campaign::load_blueprints(const Module &module)
{
	// town data
	for (const auto &town : module.board_module.towns) {
		TownBlueprint blueprint = {
			MediaManager::load_model(town.base_model),
			MediaManager::load_model(town.wall_model),
			town.label_scale
		};
		town_blueprints[town.size] = blueprint;
	}

	con_marker.model = town_blueprints[1].base_model;

	army_blueprint.model = MediaManager::load_model(module.board_module.meeple);
	// load skeletons and animations for miniatures
	army_blueprint.anim_set = std::make_unique<util::AnimationSet>();
	army_blueprint.anim_set->skeleton = MediaManager::load_skeleton(module.board_module.meeple_skeleton);
	army_blueprint.anim_set->animations[MA_IDLE] = MediaManager::load_animation(module.board_module.meeple_anim_idle);
	army_blueprint.anim_set->animations[MA_RUN] = MediaManager::load_animation(module.board_module.meeple_anim_run);
	army_blueprint.anim_set->find_max_tracks();

	// load materials for world map board
	auto &board_model = board->model();
	for (const auto &material : module.board_module.materials) {
		board_model.add_material(material.shader_name, MediaManager::load_texture(material.texture_path));
	}
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

	// new ids
	id_generator.reset();

	// generate our world
	board->generate(seed);

	// set a new clean state for game data
	const auto &atlas = board->atlas();	
	const auto &tiles = atlas.tiles();	
	for (const auto &tile : tiles) {
		faction_controller.tile_owners[tile.index] = 0;
		settlement_controller.tile_owners[tile.index] = 0;
	}

	// find ideal places to settle towns in our generated world
	faction_controller.find_town_targets(atlas, 4);

	// spawn factions including the player's
	spawn_factions();

	// spawn a meeple for every faction

	// spawn roaming meeples like bandits and barbarians
	spawn_barbarians();

	// assign player data
	player_data.meeple_id = id_generator.generate();
	meeple_controller.meeples[player_data.meeple_id] = std::make_unique<Meeple>();
	meeple_controller.player = meeple_controller.meeples[player_data.meeple_id].get();
	meeple_controller.player->id = player_data.meeple_id;
	meeple_controller.player->set_speed(4.f);
	meeple_controller.player->faction_id = player_data.faction_id;
	meeple_controller.player->control_type = MeepleControlType::PLAYER;
	meeple_controller.player->troop_count = 15;

	// position meeples at faction capitals
	for (auto &mapping : meeple_controller.meeples) {
		auto &meeple = mapping.second;
		auto faction_search = faction_controller.factions.find(meeple->faction_id);
		if (faction_search != faction_controller.factions.end()) {
			auto town_search = settlement_controller.towns.find(faction_search->second->capital_id);
			if (town_search != settlement_controller.towns.end()) {
				glm::vec2 center = board->tile_center(town_search->second->tile);
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
	board->height_field()->object()->setUserIndex2(int(CampaignEntityType::LAND_SURFACE));
	physics.add_object(board->height_field()->object(), group, mask);

	// place towns
	for (auto &town : settlement_controller.towns) {
		place_town(town.second.get());
	}

	const auto &atlas = board->atlas();	
	const auto &graph = atlas.graph();	
	const auto &tiles = atlas.tiles();	
	const auto &borders = atlas.borders();	
	const auto &cells = graph.cells;	

	// paint fiefdom tiles
	for (const auto &mapping : settlement_controller.fiefdoms) {
		const auto &fiefdom = mapping.second;
		glm::vec3 color = faction_controller.factions[fiefdom->faction()]->color();
		for (const auto &tile : fiefdom->tiles()) {
			board->paint_tile(tile, color, 1.f);
			// draw the political borders on the map
			const auto &cell = cells[tile];
			for (const auto &edge : cell.edges) {
				// find neighbor tile
				auto neighbor_index = edge->left_cell->index == tile ? edge->right_cell->index : edge->left_cell->index;
				const Tile *neighbor = &tiles[neighbor_index];
				if (faction_controller.tile_owners[neighbor->index] != fiefdom->faction()) {
					board->paint_border(edge->index, 255);
				}
			}
		}
	}

	// initial update to color the tiles
	board->update();

	// place meeples
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
	
	town_prompt.choice = TownPromptChoice::UNDECIDED;
	town_prompt.active = false;
}

// clears all the campaign data
void Campaign::clear()
{
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

	// handle town prompt
	if (town_prompt.active) {
		state = CampaignState::PAUSED;

		bool choice_made = false;
			
		if (town_prompt.choice == TownPromptChoice::REST) {
			choice_made = true;
			auto search = settlement_controller.towns.find(meeple_controller.player->target_id);
			if (search != settlement_controller.towns.end()) {
				const auto &town = search->second;
				station_meeple(meeple_controller.player, town.get());
				meeple_controller.player->clear_target();
			}
		} else if (town_prompt.choice == TownPromptChoice::VISIT) {
			choice_made = true;
			auto search = settlement_controller.towns.find(meeple_controller.player->target_id);
			if (search != settlement_controller.towns.end()) {
				const auto &town = search->second;
				battle_data.tile = town->tile;
				battle_data.town_size = town->size;
				battle_data.walled = town->walled;
				meeple_controller.player->clear_target();
			}
		} else if (town_prompt.choice == TownPromptChoice::BESIEGE) {
			choice_made = true;
			auto search = settlement_controller.towns.find(meeple_controller.player->target_id);
			if (search != settlement_controller.towns.end()) {
				const auto &town = search->second;
				transfer_town(town.get(), meeple_controller.player->faction_id);
				station_meeple(meeple_controller.player, town.get());
				meeple_controller.player->clear_target();
			}
		}

		if (choice_made) {
			if (town_prompt.choice == TownPromptChoice::VISIT) {
				state = CampaignState::BATTLE_REQUEST;
			} else {
				state = CampaignState::RUNNING;
			}
			town_prompt.choice = TownPromptChoice::UNDECIDED;
			town_prompt.active = false;
		}
	}
	
	update_camera(delta);

	//delta *= 8.f;

	// accumulate time for game tick if not paused
	if (state == CampaignState::RUNNING) {
		hourglass_sand += delta;
		// new game tick
		if (hourglass_sand > GAME_TIME_SLICE) {
			float integral = floorf(hourglass_sand);
			game_ticks += integral;
			// set internal entity ticks
			meeple_controller.add_ticks(integral);
			hourglass_sand -= integral; // reset plus fractional part
			// do updates that should only happen once per tick update
			for (auto &mapping : meeple_controller.meeples) {
				auto &meeple = mapping.second;
				update_meeple_behavior(meeple.get());
			}
			update_meeple_paths();
			// update town gameplay
			for (auto &mapping : settlement_controller.towns) {
				auto &town = mapping.second;
				update_town_tick(town.get(), integral);
			}
		}
	}

	update_debug_menu();

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
		board->set_border_mix(0.5f);
	} else {
		con_marker.visible = false;
		board->set_border_mix(0.f);
	}

	// if the game isn't paused update gameplay
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> distrib(-1.f, 1.f);
	std::uniform_real_distribution<float> distance_distrib(100.f, 300.f);

	if (state == CampaignState::RUNNING) {
		update_factions();
		meeple_controller.update(delta);
		// roaming on map
		for (auto &mapping : meeple_controller.meeples) {
			auto &meeple = mapping.second;
			if (meeple->control_type == MeepleControlType::AI_BARBARIAN && meeple->behavior_state == MeepleBehavior::PATROL) {
				if (!meeple->moving) {
					// go to new random location
					glm::vec2 direction = { distrib(gen), distrib(gen) };
					float distance = distance_distrib(gen);
					glm::vec2 destination = meeple->map_position() + distance * direction;
					std::list<glm::vec2> nodes;
					board->find_path(meeple->map_position(), destination, nodes);
					if (nodes.size()) {
						meeple->set_path(nodes);
					}
				}
			}
			update_meeple_target(meeple.get());
			// vertical offset on map
			float offset = vertical_offset(meeple->map_position());
			meeple->set_vertical_offset(offset);
		
			meeple->update_animation(delta);
		}
	}

	update_marker(meeple_controller.player->target_id, meeple_controller.player->target_type);

	// if player reached the target hide marker
	if (meeple_controller.player->target_type == 0) {
		board->hide_marker();
	}

	// find out if other armies are visible to player
	check_meeple_visibility();

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
		object_shader->uniform_mat4("MODEL", town->transform.to_matrix());
		town->model->display();
		if (town->walled) {
			town->wall_model->display();
		}
	}

	// display army entities
	meeple_shader->use();
	meeple_shader->uniform_mat4("CAMERA_VP", camera.VP);
	for (auto &mapping : meeple_controller.meeples) {
		auto &meeple = mapping.second;
		// only display if visible to player
		if (meeple->visible) {
			meeple_shader->uniform_mat4("MODEL", meeple->transform.to_matrix());
			meeple->display();
		}
	}

	if (wireframe_worldmap) {
		board->display_wireframe(camera);
	} else {
		board->display(camera);
	}
	
	if (display_debug) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		// display meeple trigger and vision sphere
		for (auto &mapping : meeple_controller.meeples) {

			auto &meeple = mapping.second;
			const auto &trigger = meeple->trigger();
			debugger->display_sphere(trigger->position(), trigger->radius());
			const auto &visibility = meeple->visibility();
			debugger->display_sphere(visibility->position(), visibility->radius());
		}
	
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		debugger->display_navmeshes();
	}

	display_labels();
}
	
void Campaign::display_labels()
{
	//glDisable(GL_DEPTH_TEST);

	label_shader->use();
	label_shader->uniform_mat4("PROJECT", camera.projection);
	label_shader->uniform_mat4("VIEW", camera.viewing);

	for (auto &mapping : meeple_controller.meeples) {
		auto &meeple = mapping.second;
		if (meeple->visible) {
			label_shader->uniform_float("SCALE", meeple->label->scale);
			label_shader->uniform_vec3("ORIGIN", meeple->transform.position + glm::vec3(0.f, 1.f, 0.f));
			label_shader->uniform_vec3("COLOR", meeple->label->background_color);
			meeple->label->background_mesh->display();
		}
	}
	for (auto &mapping : settlement_controller.towns) {
		auto &town = mapping.second;
		label_shader->uniform_float("SCALE", town->label->scale);
		label_shader->uniform_vec3("ORIGIN", town->transform.position + glm::vec3(0.f, 2.5f, 0.f));
		label_shader->uniform_vec3("COLOR", town->label->background_color);
		town->label->background_mesh->display();
	}

	font_shader->use();
	font_shader->uniform_mat4("PROJECT", camera.projection);
	font_shader->uniform_mat4("VIEW", camera.viewing);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, labeler->atlas->id);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, labeler->atlas->width, labeler->atlas->height, 0, GL_RED, GL_UNSIGNED_BYTE, labeler->atlas->data);

	for (auto &mapping : meeple_controller.meeples) {
		auto &meeple = mapping.second;
		if (meeple->visible) {
			font_shader->uniform_float("SCALE", meeple->label->scale);
			font_shader->uniform_vec3("ORIGIN", meeple->transform.position + glm::vec3(0.f, 1.f, 0.f));
			font_shader->uniform_vec3("COLOR", meeple->label->text_color);
			meeple->label->text_mesh->display();
		}
	}
	for (auto &mapping : settlement_controller.towns) {
		auto &town = mapping.second;
		font_shader->uniform_float("SCALE", town->label->scale);
		font_shader->uniform_vec3("ORIGIN", town->transform.position + glm::vec3(0.f, 2.5f, 0.f));
		font_shader->uniform_vec3("COLOR", town->label->text_color);
		town->label->text_mesh->display();
	}


	//glEnable(GL_DEPTH_TEST);
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
	float offset = vertical_offset(meeple->map_position());
	meeple->set_vertical_offset(offset);

	int meeple_mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_VISIBILITY | COLLISION_GROUP_RAY;

	auto trigger = meeple->trigger();
	trigger->ghost_object()->setUserIndex(meeple->id);
	trigger->ghost_object()->setUserIndex2(int(CampaignEntityType::MEEPLE));

	if (meeple->behavior_state != MeepleBehavior::STATIONED) {
		physics.add_object(trigger->ghost_object(), COLLISION_GROUP_INTERACTION, meeple_mask);
	}
	auto visibility = meeple->visibility();
	physics.add_object(visibility->ghost_object(), COLLISION_GROUP_VISIBILITY, COLLISION_GROUP_INTERACTION);

	meeple->model = army_blueprint.model;

	// add label
	auto faction_search = faction_controller.factions.find(meeple->faction_id);
	if (faction_search != faction_controller.factions.end()) {
		auto &faction = faction_search->second;
		meeple->label->text_color = faction->color();
	} else {
		meeple->label->text_color = { 1.f, 0.f, 0.f };
	}
	meeple->label->format("Army " + std::to_string(meeple->troop_count), labeler->font);
	meeple->label->scale = 0.1f;

	meeple->visible = false;
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

		faction->player_controlled = true;

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
		town->id = id;
		town->faction = faction->id();
		town->tile = tile->index;

		town->size = 1;
		town->walled = (tile->flags & TILE_FLAG_RIVER);
		town->troop_count = 10;

		// give town a random name
		town->name = namegen.toString();


		settlement_controller.towns[id] = std::move(town);

		faction_controller.tile_owners[tile->index] = faction->id();

		// add town to faction
		faction->add_town(id);

		return id;
	}

	return 0;
}

// spawns a new valid town entity
void Campaign::spawn_fiefdom(Town *town)
{
	int radius = 4;

	const auto faction = town->faction;

	const auto id = id_generator.generate();

	std::unique_ptr<Fiefdom> fiefdom = std::make_unique<Fiefdom>();
	fiefdom->set_id(id);
	fiefdom->set_faction(faction);
	fiefdom->set_town(town->id);

	town->fiefdom = id;

	// breadth first search
	const auto &atlas = board->atlas();	
	const auto &cells = atlas.graph().cells;	
	const auto &tiles = atlas.tiles();	
	const auto &borders = atlas.borders();	

	std::unordered_map<uint32_t, uint32_t> depth;
	std::queue<uint32_t> nodes;

	nodes.push(town->tile);
	depth[town->tile] = 0;
	faction_controller.tile_owners[town->tile] = faction;
	settlement_controller.tile_owners[town->tile] = id;

	std::vector<uint32_t> border_tiles; // tiles found at the border, very important for AI expansion and visualizing the borders

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
						settlement_controller.tile_owners[neighbor->index] = id;
						nodes.push(neighbor->index);
					}
				}
			}
		} else {
			border_tiles.push_back(node);
		}
	}

	repaint_fiefdom_tiles(fiefdom.get());

	settlement_controller.fiefdoms[id] = std::move(fiefdom);
}
	
void Campaign::wipe_fiefdom(Fiefdom *fiefdom)
{
	const auto &atlas = board->atlas();	
	const auto &cells = atlas.graph().cells;	
	const auto &tiles = atlas.tiles();	
	const auto &borders = atlas.borders();	

	// release occupied tiles
	for (const auto &tile : fiefdom->tiles()) {
		faction_controller.tile_owners[tile] = 0;
		settlement_controller.tile_owners[tile] = 0;
	}
	// wipe political borders from map
	glm::vec3 color = { 0.f, 0.f, 0.f };
	for (const auto &tile : fiefdom->tiles()) {
		board->paint_tile(tile, color, 0.f);
		const auto &cell = cells[tile];
		for (const auto &edge : cell.edges) {
			// find neighbor tile
			auto neighbor_index = edge->left_cell->index == tile ? edge->right_cell->index : edge->left_cell->index;
			const Tile *neighbor = &tiles[neighbor_index];
			const auto &neighbor_faction = faction_controller.tile_owners[neighbor->index];
			if (!neighbor_faction) {
				board->paint_border(edge->index, 0);
			} else {
				board->paint_border(edge->index, 255);
			}
		}
	}

	// finally remove it
	settlement_controller.fiefdoms.erase(fiefdom->id());
}

// place the town entity on the campaign map
void Campaign::place_town(Town *town)
{
	glm::vec2 center = board->tile_center(town->tile);
	float offset = vertical_offset(center);

	town->set_position(glm::vec3(center.x, offset, center.y));

	const auto &blueprint = town_blueprints[town->size];
	town->model = blueprint.base_model;
	town->wall_model = blueprint.wall_model;

	const int mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_VISIBILITY | COLLISION_GROUP_RAY;

	auto &trigger = town->trigger;
	trigger->ghost_object()->setUserIndex(town->id);
	trigger->ghost_object()->setUserIndex2(int(CampaignEntityType::TOWN));
	physics.add_object(trigger->ghost_object(), COLLISION_GROUP_INTERACTION, mask);

	// add label
	glm::vec3 color = faction_controller.factions[town->faction]->color();
	town->label->format(town->name + " " + std::to_string(town->troop_count), labeler->font);
	town->label->text_color = color;

	town->label->scale = blueprint.label_scale;
}
	
//  remove town entity from the campaign map
void Campaign::raze_town(Town *town)
{
	town->visible = false;

	auto &trigger = town->trigger;
	physics.remove_object(trigger->ghost_object());

	const auto &faction = faction_controller.factions[town->faction];

	// remove town from faction list
	faction->remove_town(town->id);

	// if town is faction captial find new capital
	if (town->id == faction->capital_id) {
		ConsoleManager::print("finding new capital");
		// find largest town that isn't this town
		uint8_t max_size = 0;
		uint32_t candidate_id = 0;
		for (const auto &candidate : faction->towns) {
			auto search = settlement_controller.towns.find(candidate);
			if (search != settlement_controller.towns.end()) {
				if (search->second->size > max_size) {
					max_size = search->second->size;
					candidate_id = candidate;
				}
			}
		}
		faction->capital_id = candidate_id;
	}
			
	// finally remove town
	settlement_controller.towns.erase(town->id);
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

	CampaignEntityType entity_type = CampaignEntityType(target_type);
	if (entity_type == CampaignEntityType::TOWN) {
		auto search = settlement_controller.towns.find(target_id);
		if (search != settlement_controller.towns.end()) {
			meeple->target_type = target_type;
			meeple->target_id = target_id;
		}
	} else if (entity_type == CampaignEntityType::LAND_SURFACE) {
		meeple->target_type = target_type;
	} else if (entity_type == CampaignEntityType::MEEPLE) {
		auto search = meeple_controller.meeples.find(target_id);
		if (search != meeple_controller.meeples.end()) {
			meeple->target_type = target_type;
			meeple->target_id = target_id;
		}
	}
}

BoardMarker Campaign::marker_data(const glm::vec2 &position, uint32_t target_id, uint8_t target_type)
{
	BoardMarker data = { position, glm::vec3(0.f), 1.f, 1.f };

	// if target is an entity change marker position to target center

	CampaignEntityType type = CampaignEntityType(target_type);
	if (type == CampaignEntityType::LAND_SURFACE) {
		data.color = glm::vec3(0.8f, 0.8f, 1.f);
	} else if (type == CampaignEntityType::TOWN) {
		auto search = settlement_controller.towns.find(target_id);
		if (search != settlement_controller.towns.end()) {
			auto &town = search->second;
			data.position = town->map_position();
			data.radius = 2.5f;
			if (town->faction == player_data.faction_id) {
				data.color = glm::vec3(0.f, 1.f, 0.f);
			} else {
				data.color = glm::vec3(1.f, 1.f, 0.f);
			}
		}
	} else if (type == CampaignEntityType::MEEPLE) {
		auto search = meeple_controller.meeples.find(target_id);
		if (search != meeple_controller.meeples.end()) {
			auto &meeple = search->second;
			data.position = meeple->map_position();
			data.radius = 1.f;
			if (!meeple->faction_id) {
				data.color = glm::vec3(1.f, 0.f, 0.f);
			} else if (meeple->faction_id == player_data.faction_id) {
				data.color = glm::vec3(0.f, 1.f, 0.f);
			} else {
				data.color = glm::vec3(1.f, 1.f, 0.f);
			}
		}
	}

	return data;
}

// calculates distance to target and changes game state if the target has been reached
void Campaign::update_meeple_target(Meeple *meeple)
{
	if (!meeple->target_type) {
		return;
	}

	bool reached_target = false;

	CampaignEntityType entity_type = CampaignEntityType(meeple->target_type);
	if (entity_type == CampaignEntityType::TOWN) {
		auto search = settlement_controller.towns.find(meeple->target_id);
		if (search != settlement_controller.towns.end()) {
			const auto &town = search->second;
			float distance = glm::distance(meeple->map_position(), town->map_position());
			if (distance < 1.F) {
				reached_target = true;
				meeple->behavior_state = MeepleBehavior::PATROL;
				// add town event
				if (meeple->id == player_data.meeple_id) {
					// add town prompt
					town_prompt.active = true;
					town_prompt.choice = TownPromptChoice::UNDECIDED;
				} else {
					auto &fiefdom = settlement_controller.fiefdoms[town->fiefdom];
					raze_town(town.get());
					wipe_fiefdom(fiefdom.get());
					meeple->clear_target();
				}
				//ConsoleManager::print("town action happens");
			}
		} else {
			// town not found clear target
			meeple->clear_target();
		}
	} else if (entity_type == CampaignEntityType::MEEPLE) {
		auto search = meeple_controller.meeples.find(meeple->target_id);
		if (search != meeple_controller.meeples.end()) {
			// if target is stationed ignore it
			if (search->second->behavior_state == MeepleBehavior::STATIONED) {
				meeple->clear_target();
				meeple->behavior_state = MeepleBehavior::PATROL;
				return;
			}
			float distance = glm::distance(meeple->map_position(), search->second->map_position());
			if (distance < 1.F) {
				reached_target = true;
				meeple->clear_target();
				meeple->behavior_state = MeepleBehavior::PATROL;
				// add action event
				if (meeple->id == player_data.meeple_id) {
					const Tile *tile = board->atlas().tile_at(meeple->map_position());
					battle_data.tile = tile->index;
					battle_data.town_size = 0;
					battle_data.walled = false;
					state = CampaignState::BATTLE_REQUEST;
				}
			}
		}
	} else if (entity_type == CampaignEntityType::LAND_SURFACE) {
		if (meeple->path_state() == PathState::FINISHED) {
			reached_target = true;
			meeple->clear_target();
		}
	}

	// special case where last node of path has been reached but target has not been reached yet, this happens mostly when following a moving entity
	if (!reached_target && meeple->path_state() == PathState::FINISHED) {
		update_meeple_path(meeple);
	}
}
	
// updates the visual marker on the board
void Campaign::update_marker(uint32_t target_id, uint8_t target_type)
{
	// only need to update if dynamic entity
	CampaignEntityType entity_type = CampaignEntityType(target_type);
	if (entity_type == CampaignEntityType::MEEPLE) {
		auto search = meeple_controller.meeples.find(target_id);
		if (search != meeple_controller.meeples.end()) {
			marker.position = search->second->map_position();
			board->set_marker(marker);
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

	if (town_prompt.active) {
		ImGui::Begin("Town prompt");
		ImGui::SetWindowSize(ImVec2(300, 200));
		if (ImGui::Button("visit")) {
			town_prompt.choice = TownPromptChoice::VISIT;
		}
		if (ImGui::Button("rest")) {
			town_prompt.choice = TownPromptChoice::REST;
		}
		if (ImGui::Button("besiege")) {
			town_prompt.choice = TownPromptChoice::BESIEGE;
		}
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
	float speed = 2.5f * modifier;
	if (util::InputManager::key_down(SDL_BUTTON_LEFT)) {
		glm::vec2 offset = modifier * 0.05f * util::InputManager::rel_mouse_coords();
		camera.add_offset(offset);
	}
	if (util::InputManager::key_down(SDLK_w)) { camera.move_forward(speed); }
	if (util::InputManager::key_down(SDLK_s)) { camera.move_backward(speed); }
	if (util::InputManager::key_down(SDLK_d)) { camera.move_right(speed); }
	if (util::InputManager::key_down(SDLK_a)) { camera.move_left(speed); }

	// collide with map
	float offset = vertical_offset(glm::vec2(camera.position.x, camera.position.z)) + 2.5f;
	if (camera.position.y < offset) {
		camera.position.y = offset;
	}

	camera.update_viewing();
}
	
void Campaign::set_player_movement(const glm::vec3 &ray)
{
	auto result = physics.cast_ray(camera.position, camera.position + (1000.f * ray), COLLISION_GROUP_HEIGHTMAP | COLLISION_GROUP_INTERACTION);
	if (result.hit && result.object) {
		if (result.object->getUserIndex() != player_data.meeple_id) {
			// if player is stationed in town unstation
			if (meeple_controller.player->behavior_state == MeepleBehavior::STATIONED) {
				unstation_meeple(meeple_controller.player);
			}
			set_meeple_target(meeple_controller.player, result.object->getUserIndex(), result.object->getUserIndex2());
			// find initial path
			glm::vec2 hitpoint = glm::vec2(result.point.x, result.point.z);
			marker = marker_data(hitpoint, result.object->getUserIndex(), result.object->getUserIndex2());
			std::list<glm::vec2> nodes;
			board->find_path(meeple_controller.player->map_position(), marker.position, nodes);
			// update visual marker
			// marker color is based on entity type
			if (nodes.size()) {
				marker.position = nodes.back();
				board->set_marker(marker);
				meeple_controller.player->set_path(nodes);
			
				meeple_controller.player->behavior_state == MeepleBehavior::ATTACK;

				// found a path so unpause
				// if in pause mode unpause game
				if (state == CampaignState::PAUSED) {
					state = CampaignState::RUNNING;
				}
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
	// town is already part of this faction so early exit
	if (town->faction == faction) {
		return;
	}

	// remove town from previous faction
	auto &prev_faction = faction_controller.factions[town->faction];
	prev_faction->remove_town(town->id);
	
	auto &cur_faction = faction_controller.factions[faction];

	auto &fiefdom = settlement_controller.fiefdoms[town->fiefdom];
	fiefdom->set_faction(faction);

	// add town to faction
	cur_faction->add_town(town->id);

	// transfer capital
	town->faction = faction;
	
	glm::vec3 color = faction_controller.factions[faction]->color();

	town->label->text_color = color;

	// change fiefdom tiles faction
	const auto &atlas = board->atlas();	
	const auto &graph = atlas.graph();	
	const auto &tiles = atlas.tiles();	
	const auto &borders = atlas.borders();	
	const auto &cells = graph.cells;	

	for (const auto &tile : fiefdom->tiles()) {
		faction_controller.tile_owners[tile] = faction;
	}

	repaint_fiefdom_tiles(fiefdom.get());
}
	
// updates town gameplay data by the amount of ticks
void Campaign::update_town_tick(Town *town, uint64_t ticks)
{
	const auto prev_ticks = town->ticks;
	// age town based on ticks
	town->ticks += ticks;

	bool size_changed = false;

	// grow in size
	if (prev_ticks < 60 && town->ticks >= 60) {
		size_changed = true;
		town->size = 2;
		town->troop_count = 20;
	}
	if (prev_ticks < 300 && town->ticks >= 300) {
		size_changed = true;
		town->size = 3;
		town->troop_count = 30;
	}

	if (size_changed) {
		const auto &blueprint = town_blueprints[town->size];
		town->model = blueprint.base_model;
		town->wall_model = blueprint.wall_model;
	
		town->label->scale = blueprint.label_scale;
		
		town->label->format(town->name + " " + std::to_string(town->troop_count), labeler->font);
	}
}
	
// only used in cheat mode
void Campaign::visit_current_tile()
{
	const auto &tile = board->tile_at(meeple_controller.player->map_position());

	if (tile) {
		battle_data.tile = tile->index;
		battle_data.town_size = 0;
		battle_data.walled = false;
		state = CampaignState::BATTLE_REQUEST;
	}
}
	
// updates faction treasuries based on their holdings
void Campaign::update_faction_taxes()
{
	for (const auto &mapping : faction_controller.factions) {
		auto &faction = mapping.second;
		int profit = 0;
		for (const auto &town_id : faction->towns) {
			auto search = settlement_controller.towns.find(town_id);
			if (search != settlement_controller.towns.end()) {
				const auto &town = search->second;
				profit += 8 * town->size;
			}
		}
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
		// if the faction is not already expanding or owned by player
		if (!faction->expanding && !faction->player_controlled) {
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
			uint32_t capital_tile = town_search->second->tile;
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
	
void Campaign::repaint_fiefdom_tiles(const Fiefdom *fiefdom)
{
	const auto &atlas = board->atlas();	
	const auto &cells = atlas.graph().cells;	
	const auto &tiles = atlas.tiles();	
	const auto &borders = atlas.borders();	

	// first wipe borders between same factions
	for (const auto &tile : fiefdom->tiles()) {
		// draw the political borders on the map
		const auto &cell = cells[tile];
		for (const auto &edge : cell.edges) {
			// find neighbor tile
			auto neighbor_index = edge->left_cell->index == tile ? edge->right_cell->index : edge->left_cell->index;
			const Tile *neighbor = &tiles[neighbor_index];
			if (faction_controller.tile_owners[neighbor->index] == fiefdom->faction()) {
				board->paint_border(edge->index, 0);
			}
		}
	}

	// add tile paint jobs
	glm::vec3 color = faction_controller.factions[fiefdom->faction()]->color();

	for (const auto &tile : fiefdom->tiles()) {
		board->paint_tile(tile, color, 1.f);
		// draw the political borders on the map
		const auto &cell = cells[tile];
		for (const auto &edge : cell.edges) {
			// find neighbor tile
			auto neighbor_index = edge->left_cell->index == tile ? edge->right_cell->index : edge->left_cell->index;
			const Tile *neighbor = &tiles[neighbor_index];
			if (faction_controller.tile_owners[neighbor->index] != fiefdom->faction()) {
				board->paint_border(edge->index, 255);
			}
		}
	}
}
	
// attempt to spawn a certain number of barbarians
void Campaign::spawn_barbarians()
{
	const auto &atlas = board->atlas();	
	const auto &tiles = atlas.tiles();	

	// can't do anything if no tiles
	if (!tiles.size()) {
		return;
	}

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> distrib;
	
	std::uniform_int_distribution<> troop_size_distrib(10, 20);

	// generate random points with poisson distrib
	PoissonGenerator::DefaultPRNG PRNG(distrib(gen));
	const auto positions = PoissonGenerator::generatePoissonPoints(32, PRNG, false);

	std::vector<glm::vec2> points;

	for (const auto &position : positions) {
		glm::vec2 point = { board->SCALE.x * position.x, board->SCALE.z * position.y };
		const Tile *tile = board->atlas().tile_at(point);
		if (walkable_tile(tile) && !faction_controller.tile_owners[tile->index]) {
			glm::vec2 center = board->tile_center(tile->index);
			points.push_back(center);
		}
	}
	
	// shuffle positions
	std::shuffle(points.begin(), points.end(), gen);

	for (const auto &point : points) {
		auto id = id_generator.generate();
		auto meeple = std::make_unique<Meeple>();
		meeple->id = id;
		meeple->control_type = MeepleControlType::AI_BARBARIAN;
		meeple->teleport(point);
		meeple->troop_count = troop_size_distrib(gen);
		meeple_controller.meeples[id] = std::move(meeple);
	}
}
	
void Campaign::update_meeple_paths()
{
	for (auto &mapping : meeple_controller.meeples) {
		auto &meeple = mapping.second;
		update_meeple_path(meeple.get());
	}
}

void Campaign::update_meeple_path(Meeple *meeple)
{
	CampaignEntityType entity_type = CampaignEntityType(meeple->target_type);
	// dynamic target so update path
	if (entity_type == CampaignEntityType::MEEPLE) {
		auto search = meeple_controller.meeples.find(meeple->target_id);
		if (search != meeple_controller.meeples.end()) {
			const auto &target = search->second;
			// if target is stationed ignore it
			if (target->behavior_state == MeepleBehavior::STATIONED) {
				meeple->clear_target();
				meeple->behavior_state = MeepleBehavior::PATROL;
				return;
			}
			set_path_to_entity(meeple, target.get());
		}
	} else if (entity_type == CampaignEntityType::TOWN) {
		auto search = settlement_controller.towns.find(meeple->target_id);
		if (search != settlement_controller.towns.end()) {
			const auto &target = search->second;
			set_path_to_entity(meeple, target.get());
		}
	}
}

// finds the path to an entity
void Campaign::set_path_to_entity(Meeple *meeple, const CampaignEntity *entity)
{
	std::list<glm::vec2> nodes;
	glm::vec2 end_location = entity->map_position();
	if (meeple->behavior_state == MeepleBehavior::EVADE) {
		end_location = meeple->map_position() + (meeple->map_position() - entity->map_position());
	}
	board->find_path(meeple->map_position(), end_location, nodes);
	if (nodes.size()) {
		meeple->set_path(nodes);
	}
}
	
void Campaign::update_meeple_behavior(Meeple *meeple)
{
	uint32_t weakest_target = 0;
	CampaignEntityType weakest_target_type = CampaignEntityType::INVALID;
	int weakest_target_troops = meeple->troop_count;
	uint32_t strongest_target = 0;
	CampaignEntityType strongest_target_type = CampaignEntityType::INVALID;
	glm::vec2 map_position = { 0.f, 0.f };

	if (meeple->control_type == MeepleControlType::AI_BARBARIAN && meeple->behavior_state == MeepleBehavior::PATROL) {
		// scan area for enemies
		const auto &visibility = meeple->visibility()->ghost_object();
		int count = visibility->getNumOverlappingObjects();
		for (int i = 0; i < count; i++) {
			btCollisionObject *obj = visibility->getOverlappingObject(i);
			CampaignEntityType entity_type = CampaignEntityType(obj->getUserIndex2());
			if (entity_type == CampaignEntityType::MEEPLE) {
				Meeple *target = static_cast<Meeple*>(obj->getUserPointer());
				if (target) {
					if (target->faction_id != meeple->faction_id) {
						if (target->troop_count <= weakest_target_troops) {
							weakest_target_troops = target->troop_count;
							weakest_target = target->id;
							weakest_target_type = entity_type;
							map_position = target->map_position();
						} else {
							strongest_target = target->id;
							strongest_target_type = entity_type;
						}
					}
				}
			} else if (entity_type == CampaignEntityType::TOWN) {
				Town *target = static_cast<Town*>(obj->getUserPointer());
				if (target) {
					if (target->faction != meeple->faction_id) {
						if (target->troop_count <= weakest_target_troops) {
							weakest_target_troops = target->troop_count;
							weakest_target = target->id;
							weakest_target_type = entity_type;
							map_position = target->map_position();
						}
					}
				}
			}
		}

		if (strongest_target) {
			meeple->behavior_state = MeepleBehavior::EVADE;
			set_meeple_target(meeple, strongest_target, uint8_t(strongest_target_type));
			return;
		}

		if (weakest_target) {
			// new target
			if (weakest_target != meeple->target_id) {
				meeple->behavior_state = MeepleBehavior::ATTACK;
				set_meeple_target(meeple, weakest_target, uint8_t(weakest_target_type));
				// find initial path
				std::list<glm::vec2> nodes;
				board->find_path(meeple->map_position(), map_position, nodes);
				if (nodes.size()) {
					meeple->set_path(nodes);
				}
			}
		} else {
			// didn't find any targets
			if (meeple->behavior_state == MeepleBehavior::ATTACK) {
				meeple->clear_target();
				meeple->behavior_state = MeepleBehavior::PATROL;
			}
		}
	}
}

void Campaign::check_meeple_visibility()
{
	for (auto &mapping : meeple_controller.meeples) {
		auto &meeple = mapping.second;
		meeple->visible = false;
	}
	const auto &visibility = meeple_controller.player->visibility()->ghost_object();
	int count = visibility->getNumOverlappingObjects();
	for (int i = 0; i < count; i++) {
		btCollisionObject *obj = visibility->getOverlappingObject(i);
		CampaignEntityType entity_type = CampaignEntityType(obj->getUserIndex2());
		if (entity_type == CampaignEntityType::MEEPLE) {
			Meeple *target = static_cast<Meeple*>(obj->getUserPointer());
			if (target) {
				target->visible = true;
			}
		}
	}

	// make player always visible unless stationed
	if (meeple_controller.player->behavior_state != MeepleBehavior::STATIONED) {
		meeple_controller.player->visible = true;
	}
}
	
// stations army entity in a town
void Campaign::station_meeple(Meeple *meeple, Town *town)
{
	// remove trigger
	auto trigger = meeple->trigger();
	physics.remove_object(trigger->ghost_object());

	// make invisible
	meeple->visible = false;
	
	// teleport to town center
	meeple->teleport(town->map_position());

	// set behavior state
	meeple->behavior_state = MeepleBehavior::STATIONED;
}

void Campaign::unstation_meeple(Meeple *meeple)
{
	// re-add trigger
	int meeple_mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_VISIBILITY | COLLISION_GROUP_RAY;
	auto trigger = meeple->trigger();
	physics.add_object(trigger->ghost_object(), COLLISION_GROUP_INTERACTION, meeple_mask);

	meeple->visible = true;

	// set behavior state
	meeple->behavior_state = MeepleBehavior::PATROL;
}
