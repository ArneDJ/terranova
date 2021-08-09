#pragma once
#include "atlas.h"
#include "board.h"
#include "marker.h"

class Campaign {
public:
	std::string name = {};
	util::Camera camera;
	fysx::PhysicalSystem physics;
	std::unique_ptr<Board> world;
	std::unique_ptr<gfx::SceneGroup> scene;
	Marker marker;
public:
	void load(const std::string &filepath);
	void save(const std::string &filepath);
public:
	void init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tilemap);
	void update(float delta);
	void display();
};
