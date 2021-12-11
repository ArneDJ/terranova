#pragma once
#include "atlas.h"
#include "board.h"
#include "entity.h"
#include "meeple.h"
#include "settlement.h"
#include "faction.h"

enum class CampaignState {
	RUNNING,
	PAUSED,
	BATTLE_REQUEST,
	EXIT_REQUEST
};

enum class PlayerMode {
	ARMY_MOVEMENT,
	TOWN_PLACEMENT
};

struct CampaignPlayerData {
	uint32_t faction_id = 0;
	uint32_t meeple_id = 0;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(faction_id, meeple_id);
	}
};

struct ConstructionMarker {
	geom::Transform transform;
	const gfx::Model *model = nullptr;
	bool visible = false;
};

struct CampaignBattleData {
	uint32_t tile;
	uint8_t town_size = 0;
};

struct Blueprint {
	const gfx::Model *model = nullptr;
};

class Campaign {
public:
	std::string name = {};
	CampaignState state = CampaignState::PAUSED;
	CampaignBattleData battle_data;
	int seed;
public:
	Blueprint army_blueprint;
	Blueprint town_blueprint;
public:
	util::IdGenerator id_generator;
	util::Camera camera;
	fysx::PhysicalSystem physics;
	std::unique_ptr<Board> board;
	std::unique_ptr<gfx::Labeler> labeler;
	MeepleController meeple_controller;
	SettlementController settlement_controller;
	FactionController faction_controller;
public: // entities
	ConstructionMarker con_marker;
	CampaignPlayerData player_data = {};
	PlayerMode player_mode = PlayerMode::ARMY_MOVEMENT;
public: // to keep track of game time
	float hourglass_sand = 0.f;
	uint64_t game_ticks = 0; // for every n seconds a new game tick is added
	uint64_t faction_ticks = 0;
public:
	std::shared_ptr<gfx::Shader> object_shader;
public:
	bool display_debug = false;
	bool wireframe_worldmap = false;
	std::unique_ptr<Debugger> debugger;
public:
	void load(const std::string &filepath);
	void save(const std::string &filepath);
	void generate(int seedling);
	void prepare();
	void clear();
public:
	void init(const gfx::ShaderGroup *shaders);
	void load_blueprints(const Module &module);
	void update(float delta);
	void display();
	void reset_camera();
private:
	void update_debug_menu();
	void update_camera(float delta);
	void visit_current_tile();
private:
	uint32_t spawn_town(const Tile *tile, Faction *faction);
	void spawn_fiefdom(Town *town);
	void place_town(Town *town);
	void place_meeple(Meeple *meeple);
	void set_meeple_target(Meeple *meeple, uint32_t target_id, uint8_t target_type);
	void update_meeple_target(Meeple *meeple);
	void set_player_movement(const glm::vec3 &ray);
	void set_player_construction(const glm::vec3 &ray);
	void transfer_town(Town *town, uint32_t faction);
public:
	void spawn_factions();
	void update_faction_taxes();
	void update_factions();
private:
	float vertical_offset(const glm::vec2 &position);
};
