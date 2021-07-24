#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/loguru/loguru.hpp"

#include "../geom/frustum.h"
#include "../geom/transform.h"
#include "../geom/geom.h"
#include "../util/camera.h"
#include "mesh.h"
#include "model.h"
#include "shader.h"

#include "scene.h"

namespace gfx {

IndirectMesh::IndirectMesh(const Mesh *mesh)
	: m_mesh(mesh)
{
}
	
void IndirectMesh::add(const Primitive &primitive)
{
	auto indirect_drawer = std::make_unique<IndirectDrawer>(primitive);
	m_drawers.push_back(std::move(indirect_drawer));
}
	
void IndirectMesh::add_instance()
{
	for (auto &drawer : m_drawers) {
		drawer->add_command();
	}

	m_group_count++;
}
	
void IndirectMesh::update()
{
	for (auto &drawer : m_drawers) {
		drawer->update_buffer();
	}
}

void IndirectMesh::dispatch()
{
	for (const auto &drawer : m_drawers) {
		drawer->bind_for_culling(0, 3);
		glDispatchCompute(m_group_count, 1, 1);
		glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
	}
}
	
void IndirectMesh::draw() const
{
	m_mesh->bind_vao();

	for (auto &drawer : m_drawers) {
		drawer->draw();
	}
}

SceneObject::SceneObject(const Model *model)
	: m_model(model)
{
	m_padded_transforms.buffer.set_target(GL_SHADER_STORAGE_BUFFER);
	m_model_matrices.buffer.set_target(GL_SHADER_STORAGE_BUFFER);

	for (const auto &mesh : model->meshes()) {
		auto indirect_mesh = std::make_unique<IndirectMesh>(mesh.get());
		for (const auto &primitive : mesh->primitives()) {
			indirect_mesh->add(primitive);
		}
		m_indirect_meshes.push_back(std::move(indirect_mesh));
	}
}

void SceneObject::add_transform(const geom::Transform *transform)
{
	m_transforms.push_back(transform);

	PaddedTransform padded = {
		glm::vec4(transform->position, 1.f),
		glm::vec4(transform->rotation.x, transform->rotation.y, transform->rotation.z, transform->rotation.w),
		glm::vec4(transform->scale, 1.f)
	};
	m_padded_transforms.data.push_back(padded);

	for (auto &indirect_mesh : m_indirect_meshes) {
		indirect_mesh->add_instance();
	}

	m_model_matrices.data.push_back(transform->to_matrix());

	m_instance_count++;
}

void SceneObject::update_buffers()
{
	m_padded_transforms.update_present();

	m_model_matrices.resize_necessary();

	for (auto &indirect_mesh : m_indirect_meshes) {
		indirect_mesh->update();
	}
}

uint32_t SceneObject::instance_count() const
{
	return m_instance_count;
}

void SceneObject::dispatch_frustum_cull()
{
	m_padded_transforms.buffer.bind_base(1);
	m_model_matrices.buffer.bind_base(2);

	for (auto &indirect_mesh : m_indirect_meshes) {
		indirect_mesh->dispatch();
	}
}

void SceneObject::display() const
{
	m_model_matrices.buffer.bind_base(2); // TODO use persistent-Mapping

	//m_model->bind_textures();
		
	for (const auto &indirect_mesh : m_indirect_meshes) {
		indirect_mesh->draw();
	}
}
	
SceneGroup::SceneGroup(const Shader *visual_shader, const Shader *culling_shader)
	: m_visual_shader(visual_shader), m_culling_shader(culling_shader)
{
	m_camera_ubo.set_target(GL_UNIFORM_BUFFER);
}

SceneObject* SceneGroup::find_object(const Model *model)
{
	auto search = m_objects.find(model);
	if (search != m_objects.end()) {
		return search->second.get();
	}

	// not found in the list, create a new one
	m_objects[model] = std::make_unique<SceneObject>(model);

	return m_objects[model].get();
}

void SceneGroup::update(const util::Camera &camera)
{
	struct CameraBlock block = {
		camera.viewing, camera.projection, camera.VP,
		glm::vec4(camera.position, 1.f),
		camera.frustum.planes
	};
	m_camera_ubo.store_mutable(sizeof(CameraBlock), &block, GL_STATIC_DRAW);

	for (const auto &object : m_objects) {
		object.second->update_buffers();
	}
}

void SceneGroup::cull_frustum()
{
	m_culling_shader->use();

	m_camera_ubo.bind_base(4);

	for (const auto &object : m_objects) {
		object.second->dispatch_frustum_cull();
	}
}
	
void SceneGroup::display() const
{
	m_visual_shader->use();
	
	m_camera_ubo.bind_base(4);

	for (const auto &object : m_objects) {
		object.second->display();
	}
}

};
