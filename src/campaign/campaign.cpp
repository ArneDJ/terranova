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
#include "../util/navigation.h"
#include "../util/image.h"
#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../geometry/voronoi.h"
#include "../graphics/shader.h"
#include "../graphics/mesh.h"
#include "../graphics/model.h"
#include "../graphics/scene.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"

#include "../debugger.h"
#include "../media.h"

#include "campaign.h"
	
void Campaign::init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tilemap)
{
	scene = std::make_unique<gfx::SceneGroup>(visual, culling);
	scene->set_scene_type(gfx::SceneType::DYNAMIC);
	
	board = std::make_unique<Board>(tilemap);
}
	
void Campaign::load(const std::string &filepath)
{
	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(name);
		board->load(archive);
		archive(camera);
		archive(player);
		archive(meeples);
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
		archive(player);
		archive(meeples);
	} else {
		LOG_F(ERROR, "Game saving error: could not open save file %s", filepath.c_str());
	}
}
	
void Campaign::generate(int seed)
{
	board->generate(seed);

	player.teleport(glm::vec2(512.f));
	player.set_speed(10.f);
	player.set_name("Player Army");

	// add meeples
	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> dis_x(board->BOUNDS.min.x, board->BOUNDS.max.x);
	std::uniform_real_distribution<float> dis_y(board->BOUNDS.min.y, board->BOUNDS.max.y);
	
	for (int i = 0; i < 1000; i++) {
		glm::vec2 point = { dis_x(gen), dis_y(gen) };
		auto meeple = std::make_unique<Meeple>();
		meeple->teleport(point);
		meeple->set_name("Testificate Army");
		meeples.push_back(std::move(meeple));
	}
}
	
void Campaign::prepare()
{
	// prepare camera
	camera.position = player.transform()->position + glm::vec3(0.f, 10.f, -10.f);
	camera.target(player.transform()->position);
	
	// add grapics models
	
	board->reload();

	auto cone_model = MediaManager::load_model("media/models/primitives/cone.glb");
	auto cone_object = scene->find_object(cone_model);
	cone_object->add_transform(marker.transform());

	auto duck_model = MediaManager::load_model("media/models/duck.glb");
	auto duck_object = scene->find_object(duck_model);

	player.sync();
	duck_object->add_transform(player.transform());

	for (auto &meeple : meeples) {
		meeple->sync();
		duck_object->add_transform(meeple->transform());
	}
	
	// add physical objects
	physics.add_body(board->height_field().body());
}
	
void Campaign::clear()
{
	// first clear the model instances
	scene->clear_instances();

	// clear physical objects
	physics.clear_objects();

	// clear entities
	meeples.clear();
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
