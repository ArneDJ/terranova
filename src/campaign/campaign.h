#pragma once
#include "atlas.h"
#include "board.h"
#include "meeple.h"
#include "marker.h"
#include "settlement.h"
#include "faction.h"

class Campaign {
public:
	CampaignModule module;
	std::string name = {};
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
public:
	bool display_debug = false;
	bool wireframe_worldmap = false;
	std::unique_ptr<Debugger> debugger;
public:
	void load(const std::string &filepath);
	void save(const std::string &filepath);
	void generate(int seed);
	void prepare();
	void clear();
public:
	void init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tilemap);
	void update(float delta);
	void display();
	void reset_camera();
private:
	void spawn_factions(int seed);
	uint32_t spawn_town(uint32_t tile, uint32_t faction);
	void spawn_county(Town *town);
	void place_town(Town *town);
	void place_meeple(Meeple *meeple);
private:
	float vertical_offset(const glm::vec2 &position);
};
