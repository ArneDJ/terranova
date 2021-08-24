
struct BoardMeshVertex {
	glm::vec2 position = {};
	glm::vec3 color = {};
};

class BoardMesh {
public:
	void add_cell(const geom::VoronoiCell &cell, const glm::vec3 &color);
	void create();
	void refresh();
	void draw() const;
	void clear();
	void color_tile(uint32_t tile, const glm::vec3 &color);
private:
	gfx::Primitive m_primitive;
	gfx::BufferObject m_vbo;
	gfx::BufferObject m_ebo;
	gfx::VertexArrayObject m_vao;
private:
	std::vector<BoardMeshVertex> m_vertices;
	std::unordered_map<uint32_t, std::vector<uint32_t>> m_tile_vertices;
};

class BoardModel {
public:
	BoardModel(const gfx::Shader *shader);
public:
	void reload(const Atlas &atlas);
	void color_tile(uint32_t tile, const glm::vec3 &color);
	void update_mesh();
public:
	void display(const util::Camera &camera) const;
private:
	const gfx::Shader *m_shader = nullptr;
	BoardMesh m_mesh;
};

class Board {
public:
	Board(const gfx::Shader *tilemap);
public:
	const geom::Rectangle BOUNDS = { { 0.F, 0.F }, { 1024.F, 1024.F } };
public:
	void generate(int seed);
	void reload();
	void update_model();
public:
	fysx::PlaneField& height_field();
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
	void display_wireframe(const util::Camera &camera);
private:
	int m_seed;
	Atlas m_atlas;
	BoardModel m_model;
	fysx::PlaneField m_height_field;
	util::Navigation m_land_navigation;
private:
	void build_navigation();
};
