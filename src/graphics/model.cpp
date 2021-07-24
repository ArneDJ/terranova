#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../extern/loguru/loguru.hpp"

#define CGLTF_IMPLEMENTATION

#include "../geom/transform.h"
#include "mesh.h"
#include "model.h"

namespace gfx {

static void append_buffer(const cgltf_accessor *accessor,  std::vector<uint8_t> &buffer);
static inline GLenum primitive_mode(cgltf_primitive_type type);
static void print_gltf_error(cgltf_result error);
static geom::AABB primitive_bounds(const cgltf_primitive &primitive);

Model::Model(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
{
	auto mesh = std::make_unique<Mesh>();
	mesh->create(vertices, indices);

	m_meshes.push_back(std::move(mesh));
}

Model::Model(const std::string &filepath)
{
	cgltf_options options;
	memset(&options, 0, sizeof(cgltf_options));

	cgltf_data *data = nullptr;
	cgltf_result result = cgltf_parse_file(&options, filepath.c_str(), &data);

	if (result == cgltf_result_success) {
		result = cgltf_load_buffers(&options, data, filepath.c_str());
	}

	if (result == cgltf_result_success) {
		result = cgltf_validate(data);
	}

	if (result == cgltf_result_success) {
		load(data);
	} else {
		LOG_F(ERROR, "glTF import file %s error:", filepath.c_str());
		print_gltf_error(result);
	}
		
	cgltf_free(data);
}
	
void Model::display() const
{
	for (auto &mesh : m_meshes) {
		mesh->draw();
	}
}
	
const std::vector<std::unique_ptr<Mesh>>& Model::meshes() const
{
	return m_meshes;
}

const geom::AABB& Model::bounds() const
{
	return m_bounds;
}

void Model::load(const cgltf_data *data)
{
	load_nodes(data);

	// load mesh data
	for (int i = 0; i < data->meshes_count; i++) {
		const cgltf_mesh *mesh = &data->meshes[i];
		std::string mesh_name = mesh->name ? mesh->name : std::to_string(i);
		load_visual_mesh(mesh);
	}

	// model bounding box
	m_bounds.min = glm::vec3((std::numeric_limits<float>::max)());
	m_bounds.max = glm::vec3((std::numeric_limits<float>::min)());
	for (const auto &mesh : m_meshes) {
		m_bounds = geom::confined_bounds(mesh->bounds(), m_bounds);
	}
}

void Model::load_nodes(const cgltf_data *data)
{
	std::unordered_map<const cgltf_node*, Node*> links;

	// add node data and link them
	for (int i = 0; i < data->nodes_count; i++) {
		const auto &node_data = data->nodes[i];
		auto node = std::make_unique<Node>();
		node->name = node_data.name ? node_data.name : "unnamed";

		if (node_data.has_matrix) {
			node->matrix = glm::make_mat4(node_data.matrix);
		}
		if (node_data.has_translation) {
			node->transform.position = glm::make_vec3(node_data.translation);
		}
		if (node_data.has_scale) {
			node->transform.scale = glm::make_vec3(node_data.scale);
		}
		if (node_data.has_rotation) {
			node->transform.rotation = glm::make_quat(node_data.rotation);
		}

		links[&node_data] = node.get();
		m_nodes.push_back(std::move(node));
	}

	// now that all nodes are added create the parent-child relationship
	for (int i = 0; i < data->nodes_count; i++) {
		auto search = links.find(&data->nodes[i]);
    		if (search != links.end()) {
			auto node_data = search->first;
			auto node = search->second;
			// add parent node if present
			if (node_data->parent) {
				auto parent_search = links.find(node_data->parent);
    				if (parent_search != links.end()) {
					node->parent = parent_search->second;
				}
			}
			// add children if present
			for (int j = 0; j < node_data->children_count; j++) {
				auto child_search = links.find(node_data->children[j]);
    				if (child_search != links.end()) {
					node->children.push_back(child_search->second);
				}
			}
		}
	}
}
	
void Model::load_visual_mesh(const cgltf_mesh *mesh_data)
{
	MeshBufferData buffer_data;
	std::vector<Primitive> primitives;

	uint32_t vertex_start = 0;
	uint32_t index_start = 0;

	// load the mesh primitives
	for (int i = 0; i < mesh_data->primitives_count; i++) {
		const auto &primitive_data = mesh_data->primitives[i];

		// import index data
		uint32_t index_count = 0;
		size_t index_type_size = 0;
		if (primitive_data.indices) {
			index_count = primitive_data.indices->count;
			append_buffer(primitive_data.indices, buffer_data.indices);
			index_type_size = cgltf_component_size(primitive_data.indices->component_type) * cgltf_num_components(primitive_data.indices->type);
		}

		// import vertex data
		uint32_t vertex_count = 0;
		for (int j = 0; j < primitive_data.attributes_count; j++) {
			const auto &attribute = primitive_data.attributes[j];
			const auto &accessor = attribute.data;
			switch (attribute.type) {
			case cgltf_attribute_type_position:
				vertex_count = accessor->count;
				append_buffer(accessor, buffer_data.positions);
				break;
			case cgltf_attribute_type_normal:
				append_buffer(accessor, buffer_data.normals);
				break;
			case cgltf_attribute_type_texcoord:
				append_buffer(accessor, buffer_data.texcoords);
				break;
			case cgltf_attribute_type_joints:
				append_buffer(accessor, buffer_data.joints);
				break;
			case cgltf_attribute_type_weights:
				append_buffer(accessor, buffer_data.weights);
				break;
			}
		}

		Primitive primitive;
		primitive.first_index = index_start;
		primitive.index_count = index_count;
		primitive.first_vertex = vertex_start;
		primitive.vertex_count = vertex_count;
		primitive.mode = primitive_mode(primitive_data.type);
		primitive.index_type = index_type(index_type_size);
		primitive.bounds = primitive_bounds(primitive_data);

		primitives.push_back(primitive);

		vertex_start += vertex_count;
		index_start += index_count;
	}

	auto mesh = std::make_unique<Mesh>();
	mesh->create(buffer_data, primitives);

	m_meshes.push_back(std::move(mesh));
}

// according to glTF docs: 
// A buffer is data stored as a binary blob. The buffer can contain a combination of geometry, animation, and skins.
// Binary blobs allow efficient creation of GPU buffers and textures since they require no additional parsing, except perhaps decompression.
static void append_buffer(const cgltf_accessor *accessor,  std::vector<uint8_t> &buffer)
{
	const auto view = accessor->buffer_view;
	size_t type_size = cgltf_component_size(accessor->component_type) * cgltf_num_components(accessor->type); // example: vec3 = 4 byte * 3 float

	uint8_t *data = (uint8_t*)view->buffer->data + view->offset;
	if (view->stride > 0) { // attribute data is interleaved
		for (size_t stride = accessor->offset; stride < view->size; stride += view->stride) {
			buffer.insert(buffer.end(), data + stride, data + stride + type_size);
		}
	} else { // attribute data is tightly packed
		buffer.insert(buffer.end(), data, data  + view->size);
	}
}

static inline GLenum primitive_mode(cgltf_primitive_type type)
{
	switch (type) {
	case cgltf_primitive_type_points: return GL_POINTS;
	case cgltf_primitive_type_lines: return GL_LINES;
	case cgltf_primitive_type_line_loop: return GL_LINE_LOOP;
	case cgltf_primitive_type_line_strip: return GL_LINE_STRIP;
	case cgltf_primitive_type_triangles: return GL_TRIANGLES;
	case cgltf_primitive_type_triangle_strip: return GL_TRIANGLE_STRIP;
	case cgltf_primitive_type_triangle_fan: return GL_TRIANGLE_FAN;
	default: return GL_POINTS;
	}
}

static geom::AABB primitive_bounds(const cgltf_primitive &primitive)
{
	geom::AABB bounds = {
		glm::vec3((std::numeric_limits<float>::max)()),
		glm::vec3((std::numeric_limits<float>::min)())
	};
	bool found = false;

	for (int i = 0; i < primitive.attributes_count; i++) {
		const auto &attribute = primitive.attributes[i];
		const auto &accessor = attribute.data;
		// An accessor also contains min and max properties. In the case of vertex positions, the min and max properties thus define the bounding box of an object.
		if (accessor->has_min && accessor->has_max) {
			found = true;
			bounds.min = (glm::min)(glm::make_vec3(accessor->min), bounds.min);
			bounds.max = (glm::max)(glm::make_vec3(accessor->max), bounds.max);
		}
	}

	// create standard bounds if not found
	if (!found) {
		bounds.min = glm::vec3(-1.f);
		bounds.max = glm::vec3(1.f);
	}

	return bounds;
}

static void print_gltf_error(cgltf_result error)
{
	switch (error) {
	case cgltf_result_data_too_short:
		LOG_F(ERROR, "data too short");
		break;
	case cgltf_result_unknown_format:
		LOG_F(ERROR, "unknown format");
		break;
	case cgltf_result_invalid_json:
		LOG_F(ERROR, "invalid json");
		break;
	case cgltf_result_invalid_gltf:
		LOG_F(ERROR, "invalid GLTF");
		break;
	case cgltf_result_invalid_options:
		LOG_F(ERROR, "invalid options");
		break;
	case cgltf_result_file_not_found:
		LOG_F(ERROR, "file not found");
		break;
	case cgltf_result_io_error:
		LOG_F(ERROR, "io error");
		break;
	case cgltf_result_out_of_memory:
		LOG_F(ERROR, "out of memory");
		break;
	case cgltf_result_legacy_gltf:
		LOG_F(ERROR, "legacy glTF");
		break;
	};
}

};
