#pragma once
#include "cadastre.h"
#include "landscape.h"
#include "terrain.h"

struct HouseMold {
	uint32_t id = 0;
	const gfx::Model *model = nullptr;
};

class Battle {
public:
	util::Camera camera;
	std::unique_ptr<gfx::SceneGroup> scene;
	std::unique_ptr<Terrain> terrain;
	std::unique_ptr<Debugger> debugger;
	fysx::PhysicalSystem physics;
	std::unique_ptr<fysx::Bumper> player;
	carto::Landscaper landscaper;
	std::vector<std::unique_ptr<geom::Transform>> building_transforms;
public:
	std::unordered_map<uint32_t, HouseMold> house_molds;
public:
	void init(const gfx::ShaderGroup *shaders);
	void load_molds(const Module &module);
	void prepare(int seed);
	void update(float delta);
	void display();
	void clear();
private:
	float vertical_offset(float x, float z);
};
