
class WorldModel {
public:
	WorldModel(const gfx::Shader *shader);
public:
	void reload(const Atlas &atlas, int seed);
public:
	void display(const util::Camera &camera) const;
private:
	const gfx::Shader *m_shader = nullptr;
	gfx::Mesh m_mesh;
	std::vector<gfx::Vertex> m_vertices;
	std::vector<uint32_t> m_indices;
};

class Board {
public:
	Board(const gfx::Shader *tilemap);
public:
	const geom::Rectangle BOUNDS = { { 0.F, 0.F }, { 1024.F, 1024.F } };
public:
	void generate(int seed);
	void reload();
public:
	fysx::HeightField& height_field();
	const util::Navigation& navigation() const;
	const Tile* tile_at(const glm::vec2 &position) const;
	void find_path(const glm::vec2 &start, const glm::vec2 &end, std::list<glm::vec2> &path) const;
public:
	template <class Archive>
	void save(Archive &archive) const 
	{
		archive(m_seed, m_atlas);
	}
public:
	template <class Archive>
	void load(Archive &archive)
	{
		archive(m_seed, m_atlas);
	}
public:
	void display(const util::Camera &camera);
private:
	int m_seed;
	Atlas m_atlas;
	WorldModel m_model;
	fysx::HeightField m_height_field;
	util::Navigation m_navigation;
};
