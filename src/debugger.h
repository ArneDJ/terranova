
class Debugger {
public:
	Debugger(std::shared_ptr<gfx::Shader> debug_shader);
public:
	void add_navmesh(const dtNavMesh *navmesh);
	void add_segment(const geom::Transform *a, const geom::Transform *b);
public:
	void update_camera(const util::Camera &camera);
	void clear();
	void display_navmeshes() const;
public:
	void display_sphere(const glm::vec3 &position, float radius) const;
	void display_capsule(const geom::Capsule &capsule) const;
private:
	std::shared_ptr<gfx::Shader> m_shader;
	std::vector<std::unique_ptr<gfx::Mesh>> m_navigation_meshes;
private:
	const gfx::Model *m_sphere;
	const gfx::Model *m_cube;
	const gfx::Model *m_cylinder;
};
