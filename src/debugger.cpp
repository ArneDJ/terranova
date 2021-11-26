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
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "util/camera.h"
#include "util/navigation.h"
#include "util/image.h"
#include "util/animation.h"
#include "geometry/transform.h"
#include "graphics/shader.h"
#include "graphics/mesh.h"
#include "graphics/texture.h"
#include "graphics/model.h"
#include "graphics/scene.h"

#include "media.h"
#include "debugger.h"

Debugger::Debugger(std::shared_ptr<gfx::Shader> debug_shader)
{
	m_shader = debug_shader;
	m_sphere = MediaManager::load_model("debug/shapes/sphere.glb");
	m_cube = MediaManager::load_model("debug/shapes/cube.glb");
	m_cylinder = MediaManager::load_model("debug/shapes/cylinder.glb");
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
	
void Debugger::update_camera(const util::Camera &camera)
{
	m_shader->use();
	m_shader->uniform_mat4("CAMERA_VP", camera.VP);
}
		
void Debugger::display_navmeshes() const
{
	m_shader->use();
	m_shader->uniform_bool("INDIRECT_DRAW", false);
	m_shader->uniform_bool("WIRED_MODE", false);
	m_shader->uniform_mat4("MODEL", glm::mat4(1.f));
	for (const auto &mesh : m_navigation_meshes) {
		mesh->draw();
	}
}
	
void Debugger::clear()
{
	m_navigation_meshes.clear();
}
	
void Debugger::display_sphere(const glm::vec3 &position, float radius) const
{
	m_shader->use();
	m_shader->uniform_bool("INDIRECT_DRAW", false);
	m_shader->uniform_bool("WIRED_MODE", false);

	glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(radius));
	glm::mat4 T = glm::translate(glm::mat4(1.f), position);

	m_shader->uniform_mat4("MODEL", T * S);
	m_sphere->display();
}

void Debugger::display_capsule(const geom::Capsule &capsule) const
{
	m_shader->use();
	m_shader->uniform_bool("INDIRECT_DRAW", false);
	m_shader->uniform_bool("WIRED_MODE", false);

	glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(capsule.radius));

	// first sphere
	glm::mat4 T = glm::translate(glm::mat4(1.f), capsule.a);
	m_shader->uniform_mat4("MODEL", T * S);
	m_sphere->display();

	// second sphere
	T = glm::translate(glm::mat4(1.f), capsule.b);
	m_shader->uniform_mat4("MODEL", T * S);
	m_sphere->display();

		
	// now the cylinder
	glm::vec3 position = geom::midpoint(capsule.a, capsule.b);
	glm::vec3 direction = glm::normalize(capsule.a - capsule.b);
	// assume Y-axis up
	glm::mat4 R = glm::orientation(direction, glm::vec3(0.f, 1.f, 0.f));
	T = glm::translate(glm::mat4(1.f), position);
	
	S = glm::scale(glm::mat4(1.f), glm::vec3(capsule.radius, glm::distance(position, capsule.a), capsule.radius));

	m_shader->uniform_mat4("MODEL", T * R * S);
	m_cylinder->display();
}
