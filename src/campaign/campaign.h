#pragma once
#include "atlas.h"
#include "board.h"
#include "meeple.h"
#include "marker.h"
#include "settlement.h"
#include "faction.h"

enum class CampaignState {
	RUNNING,
	PAUSED,
	BATTLE_REQUEST,
	EXIT_REQUEST
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

struct CampaignBattleData {
	uint32_t tile;
	uint8_t town_size = 0;
};

class Campaign {
public:
	std::string name = {};
	CampaignState state = CampaignState::PAUSED;
	CampaignBattleData battle_data;
	int seed;
public:
	Module module; // TODO pass this
	util::IdGenerator id_generator;
	util::Camera camera;
	fysx::PhysicalSystem physics;
	std::unique_ptr<Board> board;
	std::unique_ptr<gfx::SceneGroup> scene;
	std::unique_ptr<gfx::Labeler> labeler;
	Marker marker;
	MeepleController meeple_controller;
	SettlementController settlement_controller;
	FactionController faction_controller;
	CampaignPlayerData player_data = {};
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
	void update(float delta);
	void display();
	void reset_camera();
private:
	void spawn_factions();
	uint32_t spawn_town(uint32_t tile, uint32_t faction);
	void spawn_county(Town *town);
	void place_town(Town *town);
	void place_meeple(Meeple *meeple);
	void set_meeple_target(Meeple *meeple, uint32_t target_id, uint8_t target_type);
	void update_meeple_target(Meeple *meeple);
private:
	float vertical_offset(const glm::vec2 &position);
};
