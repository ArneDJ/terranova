#include <iostream>
#include <chrono>
#include <unordered_map>
#include <list>
#include <memory>
#include <queue>
#include <random>
#include <fstream>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/loguru/loguru.hpp"

#include "../extern/imgui/imgui.h"

#include "../extern/cereal/archives/binary.hpp"

#include "../util/serialize.h"
#include "../util/config.h"
#include "../util/input.h"
#include "../util/camera.h"
#include "../util/timer.h"
#include "../util/navigation.h"
#include "../util/image.h"
#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../geometry/voronoi.h"
#include "../graphics/shader.h"
#include "../graphics/mesh.h"
#include "../graphics/model.h"
#include "../graphics/texture.h"
#include "../graphics/scene.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"
#include "../physics/trigger.h"

#include "../debugger.h"
#include "../media.h"

#include "campaign.h"

enum CampaignCollisionGroup {
	COLLISION_GROUP_NONE = 0,
	COLLISION_GROUP_RAY = 1 << 0,
	COLLISION_GROUP_INTERACTION = 1 << 1,
	COLLISION_GROUP_TOWN = 1 << 2,
	COLLISION_GROUP_VISIBILITY = 1 << 3,
	COLLISION_GROUP_HEIGHTMAP = 1 << 4
};
	
void Campaign::init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tilemap)
{
	debugger = std::make_unique<Debugger>(visual, visual, culling);

	scene = std::make_unique<gfx::SceneGroup>(visual, culling);
	scene->set_scene_type(gfx::SceneType::DYNAMIC);
	
	board = std::make_unique<Board>(tilemap);

	meeple_controller.player = std::make_unique<Meeple>();
}
	
void Campaign::load(const std::string &filepath)
{
	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(name);
		board->load(archive);
		archive(id_generator);
		archive(camera);
		archive(meeple_controller.player);
		archive(meeple_controller.meeples);
		archive(faction_controller);
		archive(settlement_controller);
	} else {
		LOG_F(ERROR, "Game loading error: could not open save file %s", filepath.c_str());
	}
}

void Campaign::save(const std::string &filepath)
{
	std::ofstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryOutputArchive archive(stream);
		archive(name);
		board->save(archive);
		archive(id_generator);
		archive(camera);
		archive(meeple_controller.player);
		archive(meeple_controller.meeples);
		archive(faction_controller);
		archive(settlement_controller);
	} else {
		LOG_F(ERROR, "Game saving error: could not open save file %s", filepath.c_str());
	}
}
	
void Campaign::generate(int seed)
{
	id_generator.reset();

	board->generate(seed);

	meeple_controller.player = std::make_unique<Meeple>();
	meeple_controller.player->teleport(glm::vec2(512.f));
	meeple_controller.player->set_speed(10.f);
	meeple_controller.player->set_name("Player Army");

	// prepare camera
	camera.position = meeple_controller.player->transform()->position + glm::vec3(0.f, 10.f, -10.f);
	camera.target(meeple_controller.player->transform()->position);
}
	
void Campaign::prepare()
{
	board->reload();

	auto cone_model = MediaManager::load_model("media/models/primitives/cone.glb");
	auto cone_object = scene->find_object(cone_model);
	cone_object->add_transform(marker.transform());

	// add physical objects
	int group = COLLISION_GROUP_HEIGHTMAP;
	int mask = COLLISION_GROUP_RAY;
	physics.add_object(board->height_field()->object(), group, mask);

	meeple_controller.player->sync();
	add_meeple(meeple_controller.player.get());

	for (auto &meeple : meeple_controller.meeples) {
		meeple->sync();
		add_meeple(meeple.get());
	}

	board->update();
}

void Campaign::clear()
{
	// first clear the model instances
	scene->clear_instances();

	debugger->clear();

	// clear physical objects
	physics.clear_objects();

	// clear entities
	meeple_controller.clear();
	meeple_controller.player = std::make_unique<Meeple>();

	faction_controller.clear();
}

void Campaign::update(float delta)
{
	physics.update_collision_only();

	// update camera
	float modifier = 10.f * delta;
	float speed = 10.f * modifier;
	if (util::InputManager::key_down(SDL_BUTTON_LEFT)) {
		glm::vec2 offset = modifier * 0.05f * util::InputManager::rel_mouse_coords();
		camera.add_offset(offset);
	}
	if (util::InputManager::key_down(SDLK_w)) { camera.move_forward(speed); }
	if (util::InputManager::key_down(SDLK_s)) { camera.move_backward(speed); }
	if (util::InputManager::key_down(SDLK_d)) { camera.move_right(speed); }
	if (util::InputManager::key_down(SDLK_a)) { camera.move_left(speed); }

	camera.update_viewing();

	if (util::InputManager::key_pressed(SDL_BUTTON_RIGHT)) {
		glm::vec3 ray = camera.ndc_to_ray(util::InputManager::abs_mouse_coords());
		auto result = physics.cast_ray(camera.position, camera.position + (1000.f * ray));
		if (result.hit) {
			marker.teleport(result.point);
			glm::vec2 hitpoint = glm::vec2(result.point.x, result.point.z);
			std::list<glm::vec2> nodes;
			
			board->find_path(glm::vec2(meeple_controller.player->position()), hitpoint, nodes);
			meeple_controller.player->set_path(nodes);
		}
	}
	
	meeple_controller.update(delta);

	float offset = vertical_offset(meeple_controller.player->position());
	meeple_controller.player->set_vertical_offset(offset);

	debugger->update(camera);

	// campaign map paint jobs
	board->update();
}
	
void Campaign::display()
{
	scene->update(camera);
	scene->cull_frustum();
	scene->display();

	if (wireframe_worldmap) {
		board->display_wireframe(camera);
	} else {
		board->display(camera);
	}
	
	if (display_debug) {
		debugger->display_wireframe();
	}
}
	
float Campaign::vertical_offset(const glm::vec2 &position)
{
	glm::vec3 origin = { position.x, 2.F * board->SCALE.y, position.y };
	glm::vec3 end = { position.x, 0.F, position.y };

	auto result = physics.cast_ray(origin, end, COLLISION_GROUP_HEIGHTMAP);
	
	return result.point.y;
}
	
void Campaign::add_meeple(Meeple *meeple)
{
	int meeple_mask = COLLISION_GROUP_INTERACTION | COLLISION_GROUP_VISIBILITY | COLLISION_GROUP_RAY | COLLISION_GROUP_TOWN;

	auto trigger = meeple->trigger();
	physics.add_object(trigger->ghost_object(), COLLISION_GROUP_INTERACTION, meeple_mask);
	auto visibility = meeple->visibility();
	physics.add_object(visibility->ghost_object(), COLLISION_GROUP_VISIBILITY, COLLISION_GROUP_INTERACTION);

	auto duck_model = MediaManager::load_model("media/models/duck.glb");
	auto duck_object = scene->find_object(duck_model);
	duck_object->add_transform(meeple->transform());

	// debug trigger volumes
	debugger->add_sphere(trigger->form(), trigger->transform());
	debugger->add_sphere(visibility->form(), visibility->transform());
}
