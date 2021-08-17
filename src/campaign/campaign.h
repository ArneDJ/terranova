#pragma once
#include "atlas.h"
#include "board.h"
#include "meeple.h"
#include "marker.h"
#include "settlement.h"

class Campaign {
public:
	std::string name = {};
	util::Camera camera;
	fysx::PhysicalSystem physics;
	std::unique_ptr<Board> board;
	std::unique_ptr<gfx::SceneGroup> scene;
	Marker marker;
	Meeple player;
	std::unordered_map<uint32_t, std::unique_ptr<Settlement>> settlements;
public:
	void load(const std::string &filepath);
	void save(const std::string &filepath);
	void generate(int seed);
public:
	void init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tilemap);
	void update(float delta);
	void display();
};
