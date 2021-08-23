#include <iostream>
#include <random>
#include <chrono>
#include <list>
#include <unordered_map>
#include <memory>
#include <vector>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "util/camera.h"
#include "util/navigation.h"
#include "geometry/transform.h"
#include "graphics/shader.h"
#include "graphics/mesh.h"
#include "graphics/model.h"
#include "graphics/scene.h"

#include "media.h"
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
	
const geom::Transform* DebugEntity::base_transform() const
{
	return m_base_transform;
}
	
Debugger::Debugger(const gfx::Shader *debug_shader, const gfx::Shader *visual_shader, const gfx::Shader *culling_shader)
	: m_scene(visual_shader, culling_shader)
{
	m_shader = debug_shader;
	m_sphere = MediaManager::load_model("media/models/primitives/sphere.glb");
	m_cube = MediaManager::load_model("media/models/primitives/cube.glb");
	m_cylinder = MediaManager::load_model("media/models/primitives/cylinder.glb");

	m_scene.set_scene_type(gfx::SceneType::DYNAMIC);
}
	
void Debugger::add_cube(const geom::AABB &bounds, const geom::Transform *transform)
{
	const auto &shape = geom::bounds_to_transform(bounds.min, bounds.max);

	auto entity = std::make_unique<DebugEntity>(shape, transform);
	entity->update();

	auto scene_object = m_scene.find_object(m_cube);
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

	auto scene_object = m_scene.find_object(m_sphere);
	scene_object->add_transform(entity->transform());

	m_entities.push_back(std::move(entity));
}
	
void Debugger::add_cylinder(const glm::vec3 &extents, const geom::Transform *transform)
{
	geom::Transform shape;
	shape.scale = extents;

	auto entity = std::make_unique<DebugEntity>(shape, transform);
	entity->update();

	auto scene_object = m_scene.find_object(m_cylinder);
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
	
void Debugger::add_navmesh(const dtNavMesh *navmesh)
{
	std::vector<gfx::Vertex> vertices;
	std::vector<uint32_t> indices;

	std::mt19937 gen(1337);
	std::uniform_real_distribution<float> dist(0.2f, 1.f);
	glm::vec3 color = { 1.f, 0.f, 1.f };

	for (int i = 0; i < navmesh->getMaxTiles(); i++) {
		const dtMeshTile *tile = navmesh->getTile(i);
		if (!tile) { continue; }
		if (!tile->header) { continue; }
		dtPolyRef base = navmesh->getPolyRefBase(tile);
		for (int i = 0; i < tile->header->polyCount; i++) {
			const dtPoly *poly = &tile->polys[i];
			if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION) { // Skip off-mesh links.
				continue;
			}
			const dtPolyDetail *detail = &tile->detailMeshes[i];
			for (int j = 0; j < detail->triCount; j++) {
				color = glm::vec3(dist(gen), dist(gen), dist(gen));
				const uint8_t *triangle = &tile->detailTris[(detail->triBase+j)*4];
				for (int k = 0; k < 3; k++) {
					if (triangle[k] < poly->vertCount) {
						float x = tile->verts[poly->verts[triangle[k]]*3];
						float y = tile->verts[poly->verts[triangle[k]]*3 + 1] + 0.5f;
						float z = tile->verts[poly->verts[triangle[k]]*3 + 2];
						struct gfx::Vertex v = {
							{ x, y, z},
							color
						};
						vertices.push_back(v);
					} else {
						float x = tile->detailVerts[(detail->vertBase+triangle[k]-poly->vertCount)*3];
						float y = tile->detailVerts[(detail->vertBase+triangle[k]-poly->vertCount)*3 + 1];
						float z = tile->detailVerts[(detail->vertBase+triangle[k]-poly->vertCount)*3 + 2];
						struct gfx::Vertex v = {
							{ x, y, z},
							color
						};
						vertices.push_back(v);
					}
				}
			}
		}
	}

	auto mesh = std::make_unique<gfx::Mesh>();
	mesh->create(vertices, indices, GL_TRIANGLES);

	m_navigation_meshes.push_back(std::move(mesh));
}
	
void Debugger::remove_entities(const geom::Transform *transform)
{
	auto cylinder_object = m_scene.find_object(m_cylinder);
	auto sphere_object = m_scene.find_object(m_sphere);
	auto cube_object = m_scene.find_object(m_cube);

	for (auto it = m_entities.begin(); it != m_entities.end(); ) {
		auto &entity = *it;
		if (entity->base_transform() == transform) {
			cylinder_object->remove_transform(entity->transform());
			sphere_object->remove_transform(entity->transform());
			cube_object->remove_transform(entity->transform());
			it = m_entities.erase(it);
		} else {
			it++;
		}
	}
}
	
void Debugger::update(const util::Camera &camera)
{
	for (auto &entity : m_entities) {
		entity->update();
	}

	m_scene.update(camera);

	m_scene.cull_frustum();
}
		
void Debugger::display() const
{
	m_scene.display();

	m_shader->use();
	for (const auto &mesh : m_navigation_meshes) {
		mesh->draw();
	}
}

void Debugger::display_wireframe() const
{
	m_scene.display_wireframe();
}
	
void Debugger::clear()
{
	m_navigation_meshes.clear();
	m_entities.clear();
	m_scene.clear_instances();
}
