
class WorldModel {
public:
	WorldModel(const gfx::Shader *shader);
public:
	void reload(const Atlas &atlas, int seed);
	void color_tile(uint32_t tile, const glm::vec3 &color);
	void update_mesh();
public:
	void display(const util::Camera &camera) const;
private:
	const gfx::Shader *m_shader = nullptr;
	gfx::Mesh m_mesh; // TODO use a special mesh for this
	std::vector<gfx::Vertex> m_vertices;
	std::vector<uint32_t> m_indices;
	std::unordered_map<uint32_t, std::vector<uint32_t>> m_tile_vertices;
};

class Board {
public:
	Board(const gfx::Shader *tilemap);
public:
	const geom::Rectangle BOUNDS = { { 0.F, 0.F }, { 1024.F, 1024.F } };
public:
	void generate(int seed);
	void reload();
	void occupy_tiles(uint32_t start, uint32_t occupier, uint32_t radius, std::vector<uint32_t> &occupied_tiles);
	void color_tile(uint32_t tile, const glm::vec3 &color);
	void update_model();
public:
	fysx::HeightField& height_field();
	const util::Navigation& navigation() const;
	const Atlas& atlas() const { return m_atlas; }
	const Tile* tile_at(const glm::vec2 &position) const;
	glm::vec2 tile_center(uint32_t index) const;
	void find_path(const glm::vec2 &start, const glm::vec2 &end, std::list<glm::vec2> &path) const;
public:
	template <class Archive>
	void save(Archive &archive) const
	{
		archive(m_seed, m_atlas, m_land_navigation);
	}
public:
	template <class Archive>
	void load(Archive &archive)
	{
		archive(m_seed, m_atlas, m_land_navigation);
	}
public:
	void display(const util::Camera &camera);
private:
	int m_seed;
	Atlas m_atlas;
	WorldModel m_model;
	fysx::HeightField m_height_field;
	util::Navigation m_land_navigation;
};
