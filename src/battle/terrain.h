
class Terrain {
public:
	Terrain(std::shared_ptr<gfx::Shader> shader, const geom::AABB &bounds);
public:
	void generate(int seed);
	void display(const util::Camera &camera) const;
	void add_material(const std::string &name, const gfx::Texture *texture);
public:
	fysx::HeightField* height_field();
private:
	std::shared_ptr<gfx::Shader> m_shader;
	gfx::TesselationMesh m_mesh;
	gfx::Texture m_texture;
	gfx::Texture m_normal_texture;
	std::unordered_map<std::string, const gfx::Texture*> m_materials;
	glm::vec3 m_scale = {};
	util::Image<float> m_heightmap;
	util::Image<float> m_normalmap;
	std::unique_ptr<fysx::HeightField> m_height_field;
private:
	void create_normalmap();
	void bind_textures() const;
};
