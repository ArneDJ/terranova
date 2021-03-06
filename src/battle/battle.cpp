#include <iostream>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>
#include <unordered_map>
#include <list>
#include <memory>
#include <random>
#include <fstream>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/loguru/loguru.hpp"

#include "../extern/imgui/imgui.h"
#include "../extern/imgui/imgui_impl_sdl.h"
#include "../extern/imgui/imgui_impl_opengl3.h"

#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../util/input.h"
#include "../util/camera.h"
#include "../util/timer.h"
#include "../util/navigation.h"
#include "../util/image.h"
#include "../util/animation.h"
#include "../graphics/shader.h"
#include "../graphics/mesh.h"
#include "../graphics/texture.h"
#include "../graphics/model.h"
#include "../graphics/scene.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"
#include "../physics/bumper.h"

#include "../console.h"
#include "../debugger.h"
#include "../module.h"
#include "../media.h"

#include "battle.h"

enum BattleCollisionGroup {
	COLLISION_GROUP_NONE = 0,
	COLLISION_GROUP_RAY = 1 << 0,
	COLLISION_GROUP_VISIBILITY = 1 << 1,
	COLLISION_GROUP_HITBOX = 1 << 2,
	COLLISION_GROUP_WEAPON = 1 << 3,
	COLLISION_GROUP_LANDSCAPE = 1 << 4,
	COLLISION_GROUP_BUMPER = 1 << 5
};

const geom::AABB SCENE_BOUNDS = {
	{ 0.F, 0.F, 0.F },
	{ 2048.F, 255.F, 2048.F }
};
const geom::Rectangle PLAYABLE_AREA = {
	{ 512.F, 512.F },
	{ 1536.F, 1536.F }
};
  
// TODO remove
glm::vec3 creature_fly_direction(const glm::vec3 &view, bool forward, bool backward, bool right, bool left)
{
	glm::vec3 direction = { 0.f, 0.f, 0.f };
	glm::vec3 dir = glm::normalize(view);
	if (forward) {
		direction = dir;
	}
	if (backward) {
		direction = -dir;
	}
	if (right) {
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x = tmp.x;
		direction.y = 0.f;
		direction.z = tmp.z;
	}
	if (left) {
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x = -tmp.x;
		direction.y = 0.f;
		direction.z = -tmp.z;
	}

	return direction;
}

glm::vec2 creature_direction(const glm::vec3 &view, bool forward, bool backward, bool right, bool left)
{
	glm::vec2 direction = { 0.f, 0.f };
	glm::vec3 dir = glm::normalize(view);
	if (forward) {
		direction.x += dir.x;
		direction.y += dir.z;
	}
	if (backward) {
		direction.x -= dir.x;
		direction.y -= dir.z;
	}
	if (right) {
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x += tmp.x;
		direction.y += tmp.z;
	}
	if (left) {
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x -= tmp.x;
		direction.y -= tmp.z;
	}

	return direction;
}

void Battle::init(const gfx::ShaderGroup *shaders)
{
	debugger = std::make_unique<Debugger>(shaders->debug);

	scene = std::make_unique<gfx::SceneGroup>(shaders->object, shaders->culling);
	scene->set_scene_type(gfx::SceneType::FIXED);

	terrain = std::make_unique<Terrain>(shaders->terrain, SCENE_BOUNDS);
	terrain->add_material("TILES", MediaManager::load_texture("data/media/textures/terrain/tiles.dds"));

	landscaper.bounds = {
		{ SCENE_BOUNDS.min.x, SCENE_BOUNDS.min.z },
		{ SCENE_BOUNDS.max.x, SCENE_BOUNDS.max.z }
	};
	
	creature_shader = shaders->creature;

	object_shader = shaders->debug;
}

void Battle::load_molds(const Module &module)
{
	house_molds.clear();
	fort_molds.clear();

	for (const auto &architecture : module.architectures) {
		for (const auto &house : architecture.houses) {
			uint32_t id = std::hash<std::string>()(house.id);
			house_molds[id] = std::make_unique<BuildingMold>(id, MediaManager::load_model(house.model));

		}
	}

	// add house info to landscape
	for (const auto &mapping : house_molds) {
		const auto &mold = mapping.second;
		landscaper.add_house(mold->id, mold->model->bounds());
	}
	
	// load fortifications like walls and towers
	load_fort_mold(module.fortification);

	// load skeletons and animations
	anim_set = std::make_unique<util::AnimationSet>();
	anim_set->skeleton = MediaManager::load_skeleton(module.human_armature.skeleton);
	for (const auto &input : module.human_armature.animations) {
		CreatureAnimation action = CreatureAnimation(input.action_code);
		anim_set->animations[action] = MediaManager::load_animation(input.animation);
	}
	anim_set->find_max_tracks();

	// load hitboxes
	for (const auto &input : module.human_armature.hitboxes) {
		int joint_a = -1;
		int joint_b = -1;
		for (int i = 0; i < anim_set->skeleton->num_joints(); i++) {
			if (std::strstr(anim_set->skeleton->joint_names()[i], input.joint_a.c_str())) {
				joint_a = i;
				break;
			}
		}	
		for (int i = 0; i < anim_set->skeleton->num_joints(); i++) {
			if (std::strstr(anim_set->skeleton->joint_names()[i], input.joint_b.c_str())) {
				joint_b = i;
				break;
			}
		}	

		if (joint_a >= 0 && joint_b >= 0) {
			HitCapsule hitbox;
			hitbox.capsule.radius = input.radius;
			hitbox.joint_target_a = joint_a;
			hitbox.joint_target_b = joint_b;
			creature_hitboxes.push_back(hitbox);
		}
	}
}

void Battle::prepare(const BattleParameters &params)
{
	parameters = params;

	landscaper.clear();

	// local seed is based on campaign seed plus local tile index
	std::mt19937 gen(parameters.seed);
	gen.discard(parameters.tile);
	std::uniform_int_distribution<int> distrib;
	terrain->generate(distrib(gen));

	landscaper.generate(parameters.seed, parameters.tile, parameters.town_size, parameters.walled, terrain->heightmap());

	int group = COLLISION_GROUP_LANDSCAPE;
	int mask = COLLISION_GROUP_RAY | COLLISION_GROUP_BUMPER;
	physics.add_object(terrain->height_field()->object(), group, mask);

	add_houses();

	add_walls();

	// create navigation
	build_navigation();

	crowd = std::make_unique<CrowdController>(navigation.navmesh());

	// building add collision
	for (const auto &building : building_entities) {
		physics.add_body(building->body.get(), group, mask);
	}

	add_creatures();

	// grab mouse
	mousegrab = true;
	SDL_SetRelativeMouseMode(SDL_TRUE);
	
	camera_mode = BattleCamMode::THIRD_PERSON;

	debugger->add_navmesh(navigation.navmesh());
}

void Battle::clear()
{
	scene->clear_instances();

	debugger->clear();

	// clear physical objects
	physics.clear_objects();

	// clear entities
	building_entities.clear();

	creature_entities.clear();
}

void Battle::update(float delta)
{
	if (util::InputManager::key_pressed(SDLK_ESCAPE)) { 
		mousegrab = !mousegrab;
		if (mousegrab) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
		} else {
			SDL_SetRelativeMouseMode(SDL_FALSE);
		}
	}

	// camera distance third person control
	int scroll = util::InputManager::mousewheel();

	// change state
	if (camera_mode == BattleCamMode::FIRST_PERSON && scroll < 0) {
		camera_mode = BattleCamMode::THIRD_PERSON;
	}

	const float CAM_SCROLL_SPEED = 10.F;
	if (scroll > 0) {
		camera_zoom -= scroll * CAM_SCROLL_SPEED * delta;
	} else if (scroll < 0) {
		camera_zoom += (-scroll) * CAM_SCROLL_SPEED * delta;
	}

	if (camera_mode == BattleCamMode::THIRD_PERSON && camera_zoom < 0.6f) {
		camera_mode = BattleCamMode::FIRST_PERSON;
	}
	
	if (util::InputManager::key_down(SDL_BUTTON_LEFT)) {
		player->attack_request();
	}

	camera_zoom = glm::clamp(camera_zoom, 0.5f, 5.f);

	rotate_camera(delta);

	//glm::vec3 direction = creature_fly_direction(camera.direction, util::InputManager::key_down(SDLK_w), util::InputManager::key_down(SDLK_s), util::InputManager::key_down(SDLK_d), util::InputManager::key_down(SDLK_a));
	glm::vec2 dir = creature_direction(camera.direction, util::InputManager::key_down(SDLK_w), util::InputManager::key_down(SDLK_s), util::InputManager::key_down(SDLK_d), util::InputManager::key_down(SDLK_a));
	glm::vec3 direction = { dir.x, 0.f, dir.y };

	if (camera_mode == BattleCamMode::FIRST_PERSON || camera_mode == BattleCamMode::THIRD_PERSON) {
		player->look_direction = camera.direction;
		player->set_movement(direction);
	}
	player->update_collision(physics.world(), delta);

	// update bot navigation
	// TODO split in threads
	crowd->update(delta);

	auto world = physics.world();
	for (auto &creature : creature_entities) {
		// carrot on a stick
		// the carrot is the navigation agent
		// the creature will try to follow the navigation agent
		int nav_agent = creature->nav_agent;
		if (nav_agent > -1) {
			// TODO get editable agent
			glm::vec3 agent_pos = crowd->agent_position(nav_agent);
			glm::vec3 creature_pos = creature->transform->position;
			float dist = glm::distance(agent_pos, creature_pos);
			if (dist > 5.f) {
				auto target = crowd->agent_target(nav_agent);
				crowd->teleport_agent(nav_agent, creature_pos);
				crowd->retarget_agent(nav_agent, target.position, target.ref);
			}
			const float margin = 0.02f; // nav agent needs to be ahead of creature so it needs a higher speed
			crowd->set_agent_speed(nav_agent, creature->bumper->speed + margin);

			agent_pos = crowd->agent_position(nav_agent);
			glm::vec3 agent_vel = crowd->agent_velocity(nav_agent);
			creature->stick_to_agent(agent_pos, agent_vel);
			creature->look_direction = agent_vel;
		}
		creature->update_collision(physics.world(), delta);
		creature->set_leg_movement(true, false, false, false);
	}

	physics.update(delta);
		
	player->update_transform();
	player->set_leg_movement(util::InputManager::key_down(SDLK_w), util::InputManager::key_down(SDLK_s), util::InputManager::key_down(SDLK_a), util::InputManager::key_down(SDLK_d));

	for (auto &creature : creature_entities) {
		creature->update_transform();
	}
	
	player->update_animation(delta);
	player->update_hitboxes();

	// TODO optimize this
	for (auto &creature : creature_entities) {
		creature->update_animation(delta);
		creature->update_hitboxes();
	}

	if (player->attacking) {
		auto &ghost_object = player->left_fist->ghost_object;
		int count = ghost_object->getNumOverlappingObjects();
		geom::Sphere sphere = { player->left_fist->transform->position, player->left_fist->shape->getRadius() };
		for (int i = 0; i < count; i++) {
			btCollisionObject *obj = ghost_object->getOverlappingObject(i);
			Creature *target = static_cast<Creature*>(obj->getUserPointer());
			if (target) {
				if (target != player.get() && !target->dead) {
					for (auto &hitbox : target->hitboxes) {
						if (sphere_capsule_intersection(sphere, hitbox.capsule)) {
							target->kill();
							player->attacking = false;
							break;
						}
					}
				}
			}
		}
	}
	position_camera(delta);

	//debugger->update(camera);
}

void Battle::display()
{
	scene->update(camera);
	scene->cull_frustum();
	scene->display();

	/*
	for (auto &hitbox : player->hitboxes) {
		geom::Capsule scaled_capsule = hitbox.capsule;
		scaled_capsule.radius *= player->unit_scale;
		debugger->display_capsule(scaled_capsule);
	}
	*/
	debugger->update_camera(camera);
	debugger->display_sphere(player->left_fist->transform->position, player->left_fist->shape->getRadius());
	/*
	for (const auto &creature : creature_entities) {
		for (auto &hitbox : creature->hitboxes) {
			geom::Capsule scaled_capsule = hitbox.capsule;
			scaled_capsule.radius *= creature->unit_scale;
			debugger->display_capsule(scaled_capsule);
		}
	}
	*/

	creature_shader->use();
	creature_shader->uniform_mat4("CAMERA_VP", camera.VP);
	creature_shader->uniform_mat4("MODEL", player->model_transform->to_matrix());
	player->joint_matrices.buffer.bind_base(0);
	player->model->display();

	for (const auto &creature : creature_entities) {
		creature_shader->uniform_mat4("MODEL", creature->model_transform->to_matrix());
		creature->joint_matrices.buffer.bind_base(0);
		creature->model->display();
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	terrain->display(camera);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		
	//debugger->display_navmeshes();
}

float Battle::vertical_offset(float x, float z)
{
	glm::vec3 origin = { x, 2.F * SCENE_BOUNDS.max.y, z };
	glm::vec3 end = { x, 0.F, z };

	auto result = physics.cast_ray(origin, end);
	
	return result.point.y;
}
	
void Battle::update_debug_menu()
{
	ImGui::Begin("Battle Info");
	ImGui::SetWindowSize(ImVec2(300, 200));
	ImGui::Text("seed: %d", parameters.seed);
	ImGui::Text("tile: %d", parameters.tile);
	ImGui::Text("town size: %d", parameters.town_size);

	static bool freecam = false;
	ImGui::Checkbox("Freecam", &freecam);
	if (freecam) {
		camera_mode = BattleCamMode::FLYING_NOCLIP;
	} else {
		camera_mode = BattleCamMode::THIRD_PERSON;
	}

	ImGui::End();
}
	
void Battle::add_houses()
{
	for (const auto &house : landscaper.houses) {
		auto search = house_molds.find(house.second.mold_id);
		if (search != house_molds.end()) {
			auto object = scene->find_object(search->second->model);
			for (const auto &transform : house.second.transforms) {
				glm::vec3 position = { transform.position.x, 0.f, transform.position.y };
				position.y = vertical_offset(transform.position.x, transform.position.y);
				glm::quat rotation = glm::angleAxis(transform.angle, glm::vec3(0.f, 1.f, 0.f));
				auto building = std::make_unique<BuildingEntity>(position, rotation, search->second->collision->shape.get());
				building->model = search->second->model;

				object->add_transform(building->transform.get());

				building_entities.push_back(std::move(building));
			}
		}
	}
}
	
void Battle::add_walls()
{
	// add gates
	place_fort_part(landscaper.fortification.gate);
	// add towers
	place_fort_part(landscaper.fortification.tower);
	// the wall segments
	place_fort_part(landscaper.fortification.wall_even);
	place_fort_part(landscaper.fortification.wall_both);
	place_fort_part(landscaper.fortification.wall_left);
	place_fort_part(landscaper.fortification.wall_right);
	place_fort_part(landscaper.fortification.ramp);
}
	
void Battle::place_fort_part(const carto::LandscapeObject &part)
{
	auto search = fort_molds.find(part.mold_id);
	if (search != fort_molds.end()) {
		auto object = scene->find_object(search->second->model);
		for (const auto &transform : part.transforms) {
			glm::vec3 position = { transform.position.x, 0.f, transform.position.y };
			position.y = vertical_offset(transform.position.x, transform.position.y);
			glm::quat rotation = glm::angleAxis(transform.angle, glm::vec3(0.f, 1.f, 0.f));
			auto building = std::make_unique<BuildingEntity>(position, rotation, search->second->collision->shape.get());
			building->model = search->second->model;

			object->add_transform(building->transform.get());

			building_entities.push_back(std::move(building));
		}
	}
}

void Battle::add_creatures()
{
	player = std::make_unique<Creature>();
	glm::vec3 location = { 790.f, 255.f, 800.f };
	//location.y = vertical_offset(location.x, location.z) + 1.f;
	player->teleport(location);
	player->model = MediaManager::load_model("data/media/models/human.glb");
	player->set_animation(anim_set.get());

	player->set_hitbox(creature_hitboxes);

	int group = COLLISION_GROUP_BUMPER;
	int mask = COLLISION_GROUP_RAY | COLLISION_GROUP_BUMPER | COLLISION_GROUP_LANDSCAPE;

	physics.add_object(player->bumper->ghost_object.get(), group, mask);
	// add hitboxes to collision world
	physics.add_object(player->root_hitbox->ghost_object.get(), COLLISION_GROUP_HITBOX, COLLISION_GROUP_RAY | COLLISION_GROUP_WEAPON);
	physics.add_object(player->left_fist->ghost_object.get(), COLLISION_GROUP_WEAPON, COLLISION_GROUP_RAY | COLLISION_GROUP_HITBOX);


	//  TODO remove this
	glm::vec3 map_center = { 1024.f, 0.f, 1024.f };
	map_center.y = vertical_offset(map_center.x, map_center.z);

	// attackers
	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 4; j++) {
			glm::vec3 position = { 512.f + (i+i), 64.f, 512.f + (j+j) };
			position.y = vertical_offset(position.x, position.z) + 1.f;
			auto creature = std::make_unique<Creature>();
			creature->teleport(position);
			creature->model = MediaManager::load_model("data/media/models/human.glb");
			creature->set_animation(anim_set.get());
			creature->set_hitbox(creature_hitboxes);
			physics.add_object(creature->bumper->ghost_object.get(), group, mask);
			physics.add_object(creature->root_hitbox->ghost_object.get(), COLLISION_GROUP_HITBOX, COLLISION_GROUP_RAY | COLLISION_GROUP_WEAPON);

			// add navigation bot
			creature->nav_agent = crowd->add_agent(position, navigation.query());
			crowd->retarget_agent(creature->nav_agent, map_center, navigation.query());
			creature_entities.push_back(std::move(creature));
		}
	}
	// defenders
	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 4; j++) {
			glm::vec3 position = { 1536.f + (i+i), 64.f, 1536.f + (j+j) };
			position.y = vertical_offset(position.x, position.z) + 1.f;
			auto creature = std::make_unique<Creature>();
			creature->teleport(position);
			creature->model = MediaManager::load_model("data/media/models/human.glb");
			creature->set_animation(anim_set.get());
			creature->set_hitbox(creature_hitboxes);
			physics.add_object(creature->bumper->ghost_object.get(), group, mask);
			physics.add_object(creature->root_hitbox->ghost_object.get(), COLLISION_GROUP_HITBOX, COLLISION_GROUP_RAY | COLLISION_GROUP_WEAPON);

			// add navigation bot
			creature->nav_agent = crowd->add_agent(position, navigation.query());
			crowd->retarget_agent(creature->nav_agent, map_center, navigation.query());
			creature_entities.push_back(std::move(creature));
		}
	}
}
	
void Battle::rotate_camera(float delta)
{
	float modifier = 10.f * delta;
	if (mousegrab) {
		glm::vec2 offset = modifier * 0.05f * util::InputManager::rel_mouse_coords();
		camera.add_offset(offset);
	}
}
	
void Battle::position_camera(float delta)
{
	if (camera_mode == BattleCamMode::FIRST_PERSON) {
		camera.position = player->eye_position;
	} else if (camera_mode == BattleCamMode::THIRD_PERSON) {
		camera.position = player->transform->position;
		camera.position.y += 1.5f;
		camera.position -= (camera_zoom * camera.direction);
	}

	float speed = 50.f * delta;
	if (camera_mode == BattleCamMode::FLYING_NOCLIP) {
		if (util::InputManager::key_down(SDLK_w)) { camera.move_forward(speed); }
		if (util::InputManager::key_down(SDLK_s)) { camera.move_backward(speed); }
		if (util::InputManager::key_down(SDLK_d)) { camera.move_right(speed); }
		if (util::InputManager::key_down(SDLK_a)) { camera.move_left(speed); }
	}

	camera.update_viewing();
}
	
void Battle::load_fort_mold(const FortificationModule &fort)
{
	uint32_t id = 0;

	id = std::hash<std::string>()(fort.segment_even.id);
	fort_molds[id] = std::make_unique<BuildingMold>(id, MediaManager::load_model(fort.segment_even.model));
	landscaper.fortification.wall_even.mold_id = id;

	id = std::hash<std::string>()(fort.segment_both.id);
	fort_molds[id] = std::make_unique<BuildingMold>(id, MediaManager::load_model(fort.segment_both.model));
	landscaper.fortification.wall_both.mold_id = id;

	id = std::hash<std::string>()(fort.segment_left.id);
	fort_molds[id] = std::make_unique<BuildingMold>(id, MediaManager::load_model(fort.segment_left.model));
	landscaper.fortification.wall_left.mold_id = id;

	id = std::hash<std::string>()(fort.segment_right.id);
	fort_molds[id] = std::make_unique<BuildingMold>(id, MediaManager::load_model(fort.segment_right.model));
	landscaper.fortification.wall_right.mold_id = id;

	id = std::hash<std::string>()(fort.tower.id);
	fort_molds[id] = std::make_unique<BuildingMold>(id, MediaManager::load_model(fort.tower.model));
	landscaper.fortification.tower.mold_id = id;

	id = std::hash<std::string>()(fort.ramp.id);
	fort_molds[id] = std::make_unique<BuildingMold>(id, MediaManager::load_model(fort.ramp.model));
	landscaper.fortification.ramp.mold_id = id;

	id = std::hash<std::string>()(fort.gate.id);
	fort_molds[id] = std::make_unique<BuildingMold>(id, MediaManager::load_model(fort.gate.model));
	landscaper.fortification.gate.mold_id = id;
}

void Battle::build_navigation()
{
	std::vector<float> vertices;
	std::vector<int> indices;
	int index = 0;

	// navigation from heightmap
	const auto &heightmap = terrain->heightmap();
	const size_t nav_res = 512;
	const glm::vec3 scale = SCENE_BOUNDS.max - SCENE_BOUNDS.min;
	const glm::vec2 area_size = PLAYABLE_AREA.max - PLAYABLE_AREA.min;
	for (int i = 0; i < nav_res; i++) {
		for (int j = 0; j < nav_res; j++) {
			glm::vec2 relative_position = { i / float(nav_res), j / float(nav_res) };
			glm::vec2 real_position = (relative_position * area_size) + PLAYABLE_AREA.min;
			glm::vec2 image_position = { real_position.x / scale.x, real_position.y / scale.z };
			float height = heightmap.sample_relative(image_position.x, image_position.y, util::CHANNEL_RED);
			vertices.push_back(real_position.x);
			vertices.push_back(scale.y * height);
			vertices.push_back(real_position.y);
		}
	}
	for (int y = 0; y < nav_res - 1; y++) {
		for (int x = 0; x < nav_res - 1; x++) {
			int index = x + y * nav_res;

			indices.push_back(index);
			indices.push_back(index + nav_res);
			indices.push_back(index + 1);

			indices.push_back(index + 1);
			indices.push_back(index + nav_res);
			indices.push_back(index + 1 + nav_res);
		}
	}

	// navigation from buildings and other static entities
	for (const auto &building : building_entities) {
		const auto &model = building->model;
		glm::mat4 T = glm::translate(glm::mat4(1.f), building->transform->position);
		glm::mat4 R = glm::mat4(building->transform->rotation);
		glm::mat4 M = T * R;
		int index_offset = vertices.size() / 3;
		for (const auto &mesh : model->collision_meshes()) {
			for (const auto &pos : mesh->positions) {
				glm::vec4 vertex = { pos.x, pos.y, pos.z, 1.f };
				vertex = M * vertex;
				vertices.push_back(vertex.x);
				vertices.push_back(vertex.y);
				vertices.push_back(vertex.z);
			}
			for (const auto &index : mesh->indices) {
				indices.push_back(index_offset + index);
			}
		}
	}

	navigation.build(vertices, indices);
}
