#pragma once
#include "cadastre.h"
#include "landscape.h"
#include "terrain.h"
#include "mold.h"
#include "building.h"

struct BattleParameters {
	int seed;
	uint32_t tile;
	uint8_t town_size = 0;
};

class Creature {
public:
	std::unique_ptr<btConvexShape> capsule;
	std::unique_ptr<btPairCachingGhostObject> ghost_object;
	std::unique_ptr<btKinematicCharacterController> char_con;
	std::unique_ptr<geom::Transform> transform;
public:
	Creature();
	void update(const glm::vec3 &direction, bool jump_request);
	void update_transform();
};

class Battle {
public:
	BattleParameters parameters;
	util::Camera camera;
	std::unique_ptr<gfx::SceneGroup> scene;
	std::unique_ptr<Terrain> terrain;
	std::unique_ptr<Debugger> debugger;
	fysx::PhysicalSystem physics;
	std::unique_ptr<Creature> player;
	carto::Landscaper landscaper;
	std::vector<std::unique_ptr<BuildingEntity>> building_entities;
public:
	std::unordered_map<uint32_t, std::unique_ptr<HouseMold>> house_molds;
public:
	void init(const gfx::ShaderGroup *shaders);
	void load_molds(const Module &module);
	void prepare(const BattleParameters &params);
	void update(float delta);
	void display();
	void clear();
	void update_debug_menu();
private:
	float vertical_offset(float x, float z);
private:
	void add_houses();
};
