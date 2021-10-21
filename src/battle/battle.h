#pragma once
#include "cadastre.h"
#include "landscape.h"
#include "terrain.h"

class HouseMold {
public:
	uint32_t id = 0;
	const gfx::Model *model = nullptr;
	std::unique_ptr<fysx::CollisionMesh> collision;
public:
	HouseMold(uint32_t id, const gfx::Model *model);
};

class BuildingEntity {
public:
	std::unique_ptr<geom::Transform> transform;
	std::unique_ptr<btMotionState> motionstate;
	std::unique_ptr<btRigidBody> body;
public:
	BuildingEntity(const glm::vec3 &pos, const glm::quat &rot, btCollisionShape *shape);
};

struct BattleParameters {
	int seed;
	uint32_t tile;
	uint8_t town_size = 0;
};

class Battle {
public:
	BattleParameters parameters;
	util::Camera camera;
	std::unique_ptr<gfx::SceneGroup> scene;
	std::unique_ptr<Terrain> terrain;
	std::unique_ptr<Debugger> debugger;
	fysx::PhysicalSystem physics;
	std::unique_ptr<fysx::Bumper> player;
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
};
