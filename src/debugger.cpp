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

DebugEntity::DebugEntity(const geom::Transform &shape, const geom::Transform *base)
	: m_shape(shape), m_base_transform(base)
{
	m_final_transform = std::make_unique<geom::Transform>();
}

void DebugEntity::update()
{
	glm::mat4 T = glm::translate(glm::mat4(1.f), m_base_transform->position);
	glm::mat4 R = glm::mat4(m_base_transform->rotation);
	glm::mat4 S = glm::scale(glm::mat4(1.f), m_base_transform->scale);
	glm::vec4 origin = (T * R * S) * glm::vec4(m_shape.position, 1.f);

	m_final_transform->position = glm::vec3(origin.x, origin.y, origin.z);
	m_final_transform->rotation = m_base_transform->rotation;
	m_final_transform->scale = m_base_transform->scale * m_shape.scale;
}

const geom::Transform* DebugEntity::transform() const
{
	return m_final_transform.get();
}
	
Debugger::Debugger(const gfx::Shader *visual_shader, const gfx::Shader *culling_shader)
	: m_scene(visual_shader, culling_shader)
{
	m_sphere = std::make_unique<gfx::Model>("media/models/primitives/sphere.glb");
	m_cube = std::make_unique<gfx::Model>("media/models/primitives/cube.glb");
	m_cylinder = std::make_unique<gfx::Model>("media/models/primitives/cylinder.glb");
}
	
void Debugger::add_cube(const geom::AABB &bounds, const geom::Transform *transform)
{
	const auto &shape = geom::bounds_to_transform(bounds.min, bounds.max);

	auto entity = std::make_unique<DebugEntity>(shape, transform);
	entity->update();

	auto scene_object = m_scene.find_object(m_cube.get());
	scene_object->add_transform(entity->transform());

	m_entities.push_back(std::move(entity));
}
	
void Debugger::add_sphere(const geom::Sphere &sphere, const geom::Transform *transform)
{
	geom::Transform shape;
	shape.position = sphere.center;
	shape.scale = glm::vec3(sphere.radius);

	auto entity = std::make_unique<DebugEntity>(shape, transform);
	entity->update();

	auto scene_object = m_scene.find_object(m_sphere.get());
	scene_object->add_transform(entity->transform());

	m_entities.push_back(std::move(entity));
}
	
void Debugger::add_cylinder(const glm::vec3 &extents, const geom::Transform *transform)
{
	geom::Transform shape;
	shape.scale = extents;

	auto entity = std::make_unique<DebugEntity>(shape, transform);
	entity->update();

	auto scene_object = m_scene.find_object(m_cylinder.get());
	scene_object->add_transform(entity->transform());

	m_entities.push_back(std::move(entity));
}
	
void Debugger::add_capsule(float radius, float height, const geom::Transform *transform)
{
	float offset = 0.5f * height;

	geom::Sphere top = {
		glm::vec3(0.f, offset, 0.f),
		radius
	};
	add_sphere(top, transform);

	geom::Sphere bottom = {
		glm::vec3(0.f, -offset, 0.f),
		radius
	};
	add_sphere(bottom, transform);

	glm::vec3 extents = glm::vec3(radius, 0.5f * height, radius);
	add_cylinder(extents, transform);
}
	
void Debugger::update(const util::Camera &camera)
{
	for (auto &entity : m_entities) {
		entity->update();
	}

	m_scene.update_transforms();

	m_scene.update_buffers(camera);
	m_scene.cull_frustum();
}
		
void Debugger::display() const
{
	m_scene.display();
}

void Debugger::display_wireframe() const
{
	m_scene.display_wireframe();
}
