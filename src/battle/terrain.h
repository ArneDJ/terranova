
class Terrain {
public:
	Terrain(const gfx::Shader *shader);
public:
	void generate(int seed);
	void display(const util::Camera &camera) const;
private:
	const gfx::Shader *m_shader;
	gfx::TesselationMesh m_mesh;
	geom::Rectangle m_bounds = { { 0.F, 0.F }, { 1024.F, 1024.F } };
	util::Image<uint8_t> m_heightmap;
};
