namespace gpu {

struct CameraBlock {
	glm::mat4 view;
	glm::mat4 projection;
	glm::mat4 view_projection;
	glm::vec4 position;
	std::array<glm::vec4, 6> frustum;
};

class IndirectMesh {
public:
	IndirectMesh(const Mesh *mesh);
public:
	void add(const Primitive &primimtive);
	void add_instance();
	void update();
	void dispatch();
public:
	void draw() const;
private:
	const Mesh *m_mesh = nullptr;
	std::vector<std::unique_ptr<IndirectDrawer>> m_drawers;
	uint32_t m_group_count = 0;
};

class SceneObject {
public:
	SceneObject(const Model *model);
public:
	void add_transform(const geom::Transform *transform);
	void update_buffers();
	void dispatch_frustum_cull();
public:
	uint32_t instance_count() const;
	void display() const;
private:
	const Model *m_model = nullptr;
	std::vector<const geom::Transform*> m_transforms;
	uint32_t m_instance_count = 0;
	std::vector<std::unique_ptr<IndirectMesh>> m_indirect_meshes;
	BufferDataPair<PaddedTransform> m_padded_transforms;
	BufferDataPair<glm::mat4> m_model_matrices;
};

class SceneGroup {
public:
	SceneGroup(const Shader *visual_shader, const Shader *culling_shader);
public:
	SceneObject* find_object(const Model *model);
public:
	void update(const util::Camera &camera);
	void cull_frustum();
	void display() const;
private:
	std::unordered_map<const Model*, std::unique_ptr<SceneObject>> m_objects;
	gpu::BufferObject m_camera_ubo;
private:
	const Shader *m_visual_shader = nullptr;
	const Shader *m_culling_shader = nullptr;
};

};
