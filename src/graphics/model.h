#pragma once
#include "../extern/cgltf/cgltf.h"

namespace gfx {

struct Node {
	std::string name;
	glm::mat4 matrix = {};
	geom::Transform transform = {};
	Node *parent = nullptr;
	std::vector<Node*> children;
	//skin_t *skeleton = nullptr;
};

struct Skin {
	std::string name;
	std::vector<glm::mat4> inverse_binds;
	//node_t *root; // skeleton root joint
	//std::vector<node_t*> joints;
};

class Model {
public:
	Model(const std::string &filepath);
	Model(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
public:
	void display() const;
	const std::vector<std::unique_ptr<Mesh>>& meshes() const;
	const geom::AABB& bounds() const;
private:
	geom::AABB m_bounds;
	std::vector<std::unique_ptr<Node>> m_nodes;
	std::vector<std::unique_ptr<Mesh>> m_meshes;
private:
	void load(const cgltf_data *data);
	void load_nodes(const cgltf_data *data);
	void load_visual_mesh(const cgltf_mesh *mesh_data);
	//void load_collision_mesh(const cgltf_mesh *mesh_data);
	//void load_collision_hull(const cgltf_mesh *mesh_data);
};

};
