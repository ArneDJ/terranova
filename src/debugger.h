
class DebugEntity {
public:
	DebugEntity(const geom::Transform &shape, const geom::Transform *base);
public:
	void update();
	const geom::Transform* transform() const;
private:
	geom::Transform m_shape;
	const geom::Transform *m_base_transform = nullptr;
	std::unique_ptr<geom::Transform> m_final_transform;
};

class Debugger {
public:
	Debugger(const gfx::Shader *visual_shader, const gfx::Shader *culling_shader);
public:
	void add_cube(const geom::AABB &bounds, const geom::Transform *transform);
	void add_sphere(const geom::Sphere &sphere, const geom::Transform *transform);
	void add_cylinder(const glm::vec3 &extents, const geom::Transform *transform);
	void add_capsule(float radius, float height, const geom::Transform *transform);
public:
	void update(const util::Camera &camera);
	void display() const;
	void display_wireframe() const;
private:
	gfx::SceneGroup m_scene;
	std::vector<std::unique_ptr<DebugEntity>> m_entities;
private:
	std::unique_ptr<gfx::Model> m_sphere;
	std::unique_ptr<gfx::Model> m_cube;
	std::unique_ptr<gfx::Model> m_cylinder;
};