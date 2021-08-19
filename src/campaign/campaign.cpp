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

	physics.add_body(board->height_field().body());

	auto cone_model = MediaManager::load_model("media/models/primitives/cone.glb");
	auto cone_object = scene->find_object(cone_model);
	cone_object->add_transform(marker.transform());

	player.teleport(glm::vec2(512.f));
	player.set_speed(10.f);
	auto duck_model = MediaManager::load_model("media/models/duck.glb");
	auto duck_object = scene->find_object(duck_model);
	duck_object->add_transform(player.transform());

	camera.position = glm::vec3(0.f, 0.f, 0.f);
	camera.direction = glm::vec3(0.f, 1.f, 0.f);
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
	board->generate(seed);

	auto model = MediaManager::load_model("media/models/primitives/cylinder.glb");
	auto object = scene->find_object(model);
	object->clear_transforms();
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

	// add settlement
	if (util::InputManager::key_pressed(SDL_BUTTON_LEFT)) {
		// see what tile ray hits
		glm::vec3 ray = camera.ndc_to_ray(util::InputManager::abs_mouse_coords());
		auto result = physics.cast_ray(camera.position, camera.position + (1000.f * ray));
		if (result.hit) {
			//auto atlas = board->atlas();
			const auto &tile = board->tile_at(glm::vec2(result.point.x, result.point.z));
			if (tile) {
				if (tile->relief == ReliefType::LOWLAND || tile->relief == ReliefType::HILLS) {
				// if settlement not present add new one
				if (tile->occupier == 0) {
				auto search = settlements.find(tile->index);
				if (search == settlements.end()) {
					auto settlement = std::make_unique<Settlement>();
					glm::vec2 center = board->tile_center(tile->index);
					settlement->set_position(glm::vec3(center.x, 0.f, center.y));
					auto teapot_model = MediaManager::load_model("media/models/primitives/cylinder.glb");
					auto teapot_object = scene->find_object(teapot_model);
					teapot_object->add_transform(settlement->transform());
					settlements[tile->index] = std::move(settlement);
					std::vector<uint32_t> visited_tiles;
					board->occupy_tiles(tile->index, 1, 4, visited_tiles);
					for (const auto &visited : visited_tiles) {
						board->color_tile(visited, glm::vec3(1.f, 0.f, 1.f));
					}
					board->update_model();
				}
				}
				}
			}
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
