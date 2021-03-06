#pragma once
#include "../extern/cgltf/cgltf.h"

namespace gfx {

struct Node {
	std::string name;
	glm::mat4 matrix = {};
	geom::Transform transform = {};
	Node *parent = nullptr;
	std::vector<Node*> children;
};

struct Skin {
	std::string name;
	std::vector<glm::mat4> inverse_binds;
};

struct CollisionMesh {
	std::string name;
	std::vector<glm::vec3> positions;
	std::vector<uint16_t> indices;
};

class Model {
public:
	Model();
	Model(const std::string &filepath);
public:
	void add_mesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices, GLenum mode = GL_TRIANGLES);
public:
	void display() const;
	const std::vector<std::unique_ptr<Mesh>>& meshes() const;
	const std::vector<std::unique_ptr<CollisionMesh>>& collision_meshes() const;
	const std::vector<std::unique_ptr<Skin>>& skins() const;
	const geom::AABB& bounds() const;
private:
	geom::AABB m_bounds = {};
	std::vector<std::unique_ptr<Node>> m_nodes;
	std::vector<std::unique_ptr<Mesh>> m_meshes;
	std::vector<std::unique_ptr<CollisionMesh>> m_collision_meshes;
	std::vector<std::unique_ptr<Skin>> m_skins;
private:
	void load(const cgltf_data *data);
	void load_nodes(const cgltf_data *data);
	void load_visual_mesh(const cgltf_mesh *mesh_data);
	void load_collision_mesh(const cgltf_mesh *mesh_data);
	//void load_collision_hull(const cgltf_mesh *mesh_data);
	void load_skin(const cgltf_skin *skin_data);
private:
	void calculate_bounds();
};

};
