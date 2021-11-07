#pragma once
#include "cadastre.h"
#include "landscape.h"
#include "terrain.h"
#include "mold.h"
#include "building.h"
#include "creature.h"

struct BattleParameters {
	int seed;
	uint32_t tile;
	uint8_t town_size = 0;
};

enum class BattlePlayerMode {
	FLYING_CAMERA_NOCLIP,
	FLYING_CAMERA_CLIP,
	CREATURE_CONTROL
};

class Battle {
public:
	BattleParameters parameters;
	std::unique_ptr<gfx::SceneGroup> scene;
	std::unique_ptr<Terrain> terrain;
	std::unique_ptr<Debugger> debugger;
	fysx::PhysicalSystem physics;
	carto::Landscaper landscaper;
	std::vector<std::unique_ptr<BuildingEntity>> building_entities;
	std::vector<std::unique_ptr<Creature>> creature_entities;
	std::shared_ptr<gfx::Shader> creature_shader;
public:
	BattlePlayerMode player_mode = BattlePlayerMode::CREATURE_CONTROL;
	util::Camera camera;
	std::unique_ptr<Creature> player;
	bool mousegrab = true;
public:
	std::unordered_map<uint32_t, std::unique_ptr<HouseMold>> house_molds;
	const ozz::animation::Skeleton *skeleton;
	const ozz::animation::Animation *animation;
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
	void rotate_camera(float delta);
	void position_camera(float delta);
private:
	void add_houses();
	void add_creatures();
};
