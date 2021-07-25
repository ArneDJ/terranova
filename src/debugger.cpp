#include <iostream>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <vector>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "util/camera.h"
#include "geometry/transform.h"
#include "graphics/shader.h"
#include "graphics/mesh.h"
#include "graphics/model.h"
#include "graphics/scene.h"

#include "debugger.h"
	
Debugger::Debugger(const gfx::Shader *visual_shader, const gfx::Shader *culling_shader)
	: m_scene(visual_shader, culling_shader)
{
	// TODO generate instead of import?
	m_sphere = std::make_unique<gfx::Model>("media/models/primitives/sphere.glb");
	m_cube = std::make_unique<gfx::Model>("media/models/primitives/cube.glb");
}
	
void Debugger::add_cube(const geom::AABB &bounds, const geom::Transform *transform)
{
	auto entity = std::make_unique<DebugEntity>();

	entity->shape = geom::bounds_to_transform(bounds.min, bounds.max);

	entity->original = transform;

	// FIXME put in function
	entity->final_transform = std::make_unique<geom::Transform>();
	entity->final_transform->position = transform->position + (transform->scale * entity->shape.position);
	entity->final_transform->rotation = transform->rotation;
	entity->final_transform->scale = entity->shape.scale * transform->scale;
	
	auto scene_object = m_scene.find_object(m_cube.get());
	scene_object->add_transform(entity->final_transform.get());

	m_entities.push_back(std::move(entity));
}
	
void Debugger::add_sphere(const geom::Sphere &sphere, const geom::Transform *transform)
{
	auto entity = std::make_unique<DebugEntity>();

	entity->shape.position = sphere.center;
	entity->shape.scale = glm::vec3(sphere.radius);

	entity->original = transform;

	// FIXME put in function
	entity->final_transform = std::make_unique<geom::Transform>();
	entity->final_transform->position = transform->position + (transform->scale * entity->shape.position);
	entity->final_transform->rotation = transform->rotation;
	entity->final_transform->scale = entity->shape.scale * transform->scale;
	
	auto scene_object = m_scene.find_object(m_sphere.get());
	scene_object->add_transform(entity->final_transform.get());

	m_entities.push_back(std::move(entity));
}
	
void Debugger::update(const util::Camera &camera)
{
	for (auto &entity : m_entities) {
	// FIXME put in function
		entity->final_transform->position = entity->original->position + (entity->original->scale * entity->shape.position);
		entity->final_transform->rotation = entity->original->rotation;
		entity->final_transform->scale = entity->shape.scale * entity->original->scale;
	}
	m_scene.update_transforms();

	m_scene.update_buffers(camera);
	m_scene.cull_frustum();
}
		
void Debugger::display()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	m_scene.display();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
