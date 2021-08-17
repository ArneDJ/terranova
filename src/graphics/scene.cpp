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

#include "../geometry/frustum.h"
#include "../geometry/transform.h"
#include "../geometry/geometry.h"
#include "../util/camera.h"
#include "mesh.h"
#include "model.h"
#include "shader.h"

#include "scene.h"

namespace gfx {

struct BindingBlock {
	const GLchar *name;
	GLuint index;
};

// shader storage buffers
static const BindingBlock BLOCK_COMMANDS = { "DrawCommandsBlock", 0 };
static const BindingBlock BLOCK_PADDED_TRANSFORMS = { "PaddedTransformBlock", 1 };
static const BindingBlock BLOCK_MODEL_MATRICES = { "ModelMatricesBlock", 2 };

// uniform buffers 
static const BindingBlock BLOCK_CULL_SHAPE = { "CullShapeBlock", 3 };
static const BindingBlock BLOCK_CAMERA = { "CameraBlock", 4 };

IndirectMesh::IndirectMesh(const Mesh *mesh)
	: m_mesh(mesh)
{
}
	
void IndirectMesh::add(const Primitive &primitive)
{
	if (primitive.index_count > 0) {
		auto drawer = std::make_unique<IndirectElementsDrawer>(primitive);
		m_elements_drawers.push_back(std::move(drawer));
	} else {
		auto drawer = std::make_unique<IndirectDrawer>(primitive);
		m_drawers.push_back(std::move(drawer));
	}
}
	
void IndirectMesh::add_instance()
{
	for (auto &drawer : m_drawers) {
		drawer->add_command();
	}
	for (auto &drawer : m_elements_drawers) {
		drawer->add_command();
	}

	m_group_count++;
}
	
void IndirectMesh::remove_instance()
{
	for (auto &drawer : m_drawers) {
		drawer->pop_command();
	}
	for (auto &drawer : m_elements_drawers) {
		drawer->pop_command();
	}

	m_group_count--;
}
	
void IndirectMesh::clear_instances()
{
	for (auto &drawer : m_drawers) {
		drawer->clear_commands();
	}
	for (auto &drawer : m_elements_drawers) {
		drawer->clear_commands();
	}

	m_group_count = 0;
}
	
void IndirectMesh::update()
{
	for (auto &drawer : m_drawers) {
		drawer->update_buffer();
	}
	for (auto &drawer : m_elements_drawers) {
		drawer->update_buffer();
	}
}

void IndirectMesh::dispatch()
{
	for (const auto &drawer : m_drawers) {
		drawer->bind_for_culling(BLOCK_COMMANDS.index, BLOCK_CULL_SHAPE.index);
		glDispatchCompute(m_group_count, 1, 1);
		glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
	}
	for (const auto &drawer : m_elements_drawers) {
		drawer->bind_for_culling(BLOCK_COMMANDS.index, BLOCK_CULL_SHAPE.index);
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
	for (auto &drawer : m_elements_drawers) {
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
	
void SceneObject::remove_transform(const geom::Transform *transform)
{
	for (int i = 0; i < m_transforms.size(); i++) {
		if (m_transforms[i] == transform) {
			m_transforms.erase(m_transforms.begin() + i);
			m_padded_transforms.erase(i);
			m_model_matrices.erase(i);

			for (auto &indirect_mesh : m_indirect_meshes) {
				indirect_mesh->remove_instance();
			}

			m_instance_count--;

			break;
		}
	}
}
	
void SceneObject::clear_transforms()
{
	m_transforms.clear();
	m_padded_transforms.clear();
	m_model_matrices.clear();

	for (auto &indirect_mesh : m_indirect_meshes) {
		indirect_mesh->clear_instances();
	}
}
	
void SceneObject::update_transforms()
{
	if (m_padded_transforms.data.size() == m_transforms.size()) {
		for (int i = 0; i < m_transforms.size(); i++) {
			m_padded_transforms.data[i].position.x = m_transforms[i]->position.x;
			m_padded_transforms.data[i].position.y = m_transforms[i]->position.y;
			m_padded_transforms.data[i].position.z = m_transforms[i]->position.z;
			m_padded_transforms.data[i].rotation.x = m_transforms[i]->rotation.x;
			m_padded_transforms.data[i].rotation.y = m_transforms[i]->rotation.y;
			m_padded_transforms.data[i].rotation.z = m_transforms[i]->rotation.z;
			m_padded_transforms.data[i].rotation.w = m_transforms[i]->rotation.w;
			m_padded_transforms.data[i].scale.x = m_transforms[i]->scale.x;
			m_padded_transforms.data[i].scale.y = m_transforms[i]->scale.y;
			m_padded_transforms.data[i].scale.z = m_transforms[i]->scale.z;
		}
	}
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
	m_padded_transforms.buffer.bind_base(BLOCK_PADDED_TRANSFORMS.index);
	m_model_matrices.buffer.bind_base(BLOCK_MODEL_MATRICES.index);

	for (auto &indirect_mesh : m_indirect_meshes) {
		indirect_mesh->dispatch();
	}
}

void SceneObject::display() const
{
	m_model_matrices.buffer.bind_base(BLOCK_MODEL_MATRICES.index);

	//m_model->bind_textures();
		
	for (const auto &indirect_mesh : m_indirect_meshes) {
		indirect_mesh->draw();
	}
}
	
SceneGroup::SceneGroup(const Shader *visual_shader, const Shader *culling_shader)
	: m_visual_shader(visual_shader), m_culling_shader(culling_shader)
{
	m_camera_ubo.set_target(GL_UNIFORM_BUFFER);

	culling_shader->set_storage_block(BLOCK_COMMANDS.name, BLOCK_COMMANDS.index);
	culling_shader->set_storage_block(BLOCK_PADDED_TRANSFORMS.name, BLOCK_PADDED_TRANSFORMS.index);
	culling_shader->set_storage_block(BLOCK_MODEL_MATRICES.name, BLOCK_MODEL_MATRICES.index);
	
	culling_shader->set_uniform_block(BLOCK_CULL_SHAPE.name, BLOCK_CULL_SHAPE.index);
	culling_shader->set_uniform_block(BLOCK_CAMERA.name, BLOCK_CAMERA.index);
	
	visual_shader->set_storage_block(BLOCK_MODEL_MATRICES.name, BLOCK_MODEL_MATRICES.index);
	visual_shader->set_uniform_block(BLOCK_CAMERA.name, BLOCK_CAMERA.index);
}
	
void SceneGroup::set_scene_type(enum SceneType type)
{
	m_scene_type = type;
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
	if (m_scene_type == SceneType::DYNAMIC) {
		for (const auto &object : m_objects) {
			object.second->update_transforms();
		}
	}

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

	m_camera_ubo.bind_base(BLOCK_CAMERA.index);

	for (const auto &object : m_objects) {
		object.second->dispatch_frustum_cull();
	}
}
	
void SceneGroup::display() const
{
	m_visual_shader->use();

	m_visual_shader->uniform_bool("INDIRECT_DRAW", true);
	m_visual_shader->uniform_bool("WIRED_MODE", false);
	
	m_camera_ubo.bind_base(BLOCK_CAMERA.index);

	for (const auto &object : m_objects) {
		object.second->display();
	}
}

void SceneGroup::display_wireframe() const
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	m_visual_shader->use();

	m_visual_shader->uniform_bool("INDIRECT_DRAW", true);
	m_visual_shader->uniform_bool("WIRED_MODE", true);
	
	m_camera_ubo.bind_base(BLOCK_CAMERA.index);

	for (const auto &object : m_objects) {
		object.second->display();
	}
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
	
void SceneGroup::clear_instances()
{
	for (auto &object : m_objects) {
		object.second->clear_transforms();
	}
}

};
