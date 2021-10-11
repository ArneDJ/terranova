#pragma once
#include "atlas.h"
#include "board.h"
#include "meeple.h"
#include "marker.h"
#include "settlement.h"
#include "faction.h"

class Campaign {
public:
	std::string name = {};
	util::IdGenerator id_generator;
	util::Camera camera;
	fysx::PhysicalSystem physics;
	std::unique_ptr<Board> board;
	std::unique_ptr<gfx::SceneGroup> scene;
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
private:
	void prepare_collision();
	void prepare_graphics();
private:
	float vertical_offset(const glm::vec2 &position);
};
