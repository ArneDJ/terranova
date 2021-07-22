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
#include "../geom/geom.h"
#include "mesh.h"
#include "model.h"

namespace gpu {

static void print_gltf_error(cgltf_result error);

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
		//find_bounds(data);
	} else {
		LOG_F(ERROR, "glTF import file %s error:", filepath.c_str());
		print_gltf_error(result);
	}
		
	cgltf_free(data);
}

void Model::load(const cgltf_data *data)
{
	load_nodes(data);
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

	/*
	for (const auto &node : m_nodes) {
		std::cout << "Node: " + node->name << std::endl;
		if (node->parent) {
			std::cout << "Node parent: " << node->parent->name << std::endl;
		}
		for (const auto &child : node->children) {
			std::cout << "Node child: " << child->name << std::endl;
		}
	}
	*/
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
