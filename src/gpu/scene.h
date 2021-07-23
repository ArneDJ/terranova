namespace gpu {

// TODO rename to IndirectMesh
class IndirectObject {
public:
	IndirectObject(const Mesh *mesh);
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
	std::vector<std::unique_ptr<IndirectObject>> m_indirect_meshes;
	BufferDataPair<PaddedTransform> m_padded_transforms;
	BufferDataPair<glm::mat4> m_model_matrices;
};

class SceneGroup {
public:
	SceneObject* find_object(const Model *model);
public:
	void update();
	void cull_frustum();
	void display() const;
private:
	std::unordered_map<const Model*, std::unique_ptr<SceneObject>> m_objects;
};

};
