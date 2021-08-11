#include <iostream>
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

#include "../extern/cereal/archives/binary.hpp"

#include "../util/serialize.h"
#include "../util/config.h"
#include "../util/input.h"
#include "../util/camera.h"
#include "../util/timer.h"
#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../geometry/voronoi.h"
#include "../graphics/shader.h"
#include "../graphics/mesh.h"
#include "../graphics/model.h"
#include "../graphics/scene.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"
#include "../navigation/astar.h"

#include "../debugger.h"
#include "../media.h"

#include "campaign.h"
	
void Campaign::init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tilemap)
{
	scene = std::make_unique<gfx::SceneGroup>(visual, culling);
	scene->set_scene_type(gfx::SceneType::DYNAMIC);
	
	board = std::make_unique<Board>(tilemap);

	physics.add_body(board->height_field().body());

	auto cone_model = MediaManager::load_model("media/models/primitives/cone.glb");
	auto cone_object = scene->find_object(cone_model);
	cone_object->add_transform(marker.transform());

	player.teleport(glm::vec2(0.f));
	player.set_speed(10.f);
	auto duck_model = MediaManager::load_model("media/models/duck.glb");
	auto duck_object = scene->find_object(duck_model);
	duck_object->add_transform(player.transform());
}
	
void Campaign::load(const std::string &filepath)
{
	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(name);
		board->load(archive);
		archive(camera);
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
		archive(camera);
	} else {
		LOG_F(ERROR, "Game saving error: could not open save file %s", filepath.c_str());
	}
}
	
void Campaign::generate(int seed)
{
	remove_teapots();
	board->generate(seed);
}

// FIXME 
void Campaign::add_teapots(const std::list<glm::vec2> &nodes)
{
	auto model = MediaManager::load_model("media/models/teapot.glb");
	auto object = scene->find_object(model);

	// remove previous nodes
	remove_teapots();

	for (const auto &point : nodes) {
		auto transform = std::make_unique<geom::Transform>();
		transform->position.x = point.x;
		transform->position.y = 10.f;
		transform->position.z = point.y;
		object->add_transform(transform.get());

		teapots.push_back(std::move(transform));
	}
}

// FIXME 
void Campaign::remove_teapots()
{
	auto model = MediaManager::load_model("media/models/teapot.glb");
	auto object = scene->find_object(model);

	for (auto &transform : teapots) {
		object->remove_transform(transform.get());
	}
	teapots.clear();
}
	
void Campaign::update(float delta)
{
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
			std::list<glm::vec2> nodes;
			
			board->find_path(glm::vec2(player.transform()->position.x, player.transform()->position.z), glm::vec2(result.point.x, result.point.z), nodes);
			add_teapots(nodes);
			player.set_path(nodes);
		}
	}

	player.update(delta);
}
	
void Campaign::display()
{
	scene->update(camera);
	scene->cull_frustum();
	scene->display();

	board->display(camera);
}
