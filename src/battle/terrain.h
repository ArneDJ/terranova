
class Terrain {
public:
	Terrain(std::shared_ptr<gfx::Shader> shader);
public:
	void generate(int seed);
	void display(const util::Camera &camera) const;
public:
	fysx::HeightField* height_field();
private:
	std::shared_ptr<gfx::Shader> m_shader;
	gfx::TesselationMesh m_mesh;
	gfx::Texture m_texture;
	glm::vec3 m_scale = { 1024.f, 64.f, 1024.f };
	util::Image<uint8_t> m_heightmap;
	std::unique_ptr<fysx::HeightField> m_height_field;
};
