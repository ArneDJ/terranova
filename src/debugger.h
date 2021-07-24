
struct DebugEntity {
	geom::Transform shape;
	const geom::Transform *original = nullptr;
	std::unique_ptr<geom::Transform> final_transform;
};

class Debugger {
public:
	Debugger(const gfx::Shader *visual_shader, const gfx::Shader *culling_shader);
public:
	void add_cube(const geom::AABB &bounds, const geom::Transform *transform);
	void add_sphere(const geom::Sphere &sphere, const geom::Transform *transform);
public:
	void update(const util::Camera &camera);
	void display();
private:
	gfx::SceneGroup m_scene;
	std::vector<std::unique_ptr<DebugEntity>> m_entities;
private:
	std::unique_ptr<gfx::Model> m_sphere;
	std::unique_ptr<gfx::Model> m_cube;
};
