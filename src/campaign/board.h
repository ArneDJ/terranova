
struct BoardMarker {
	glm::vec2 position = {};
	glm::vec3 color = {};
	float radius = 3.f;
	float fade = 1.f; // transparancy, keep this between 0 and 1
};

class BoardModel {
public:
	BoardModel(std::shared_ptr<gfx::Shader> shader, std::shared_ptr<gfx::Shader> blur_shader, const util::Image<float> &heightmap, const util::Image<float> &normalmap);
public:
	void set_scale(const glm::vec3 &scale);
	void add_material(const std::string &name, const gfx::Texture *texture);
	void reload(const Atlas &atlas);
	void paint_political_triangle(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec3 &color);
	void paint_political_line(const glm::vec2 &a, const glm::vec2 &b, uint8_t color);
	void update();
public:
	void set_marker(const BoardMarker &marker);
	void hide_marker();
public:
	void set_border_mix(float mix);
public:
	void display(const util::Camera &camera) const;
	void bind_textures() const;
private:
	std::shared_ptr<gfx::Shader> m_shader;
	std::shared_ptr<gfx::Shader> m_blur_shader;
private:
	glm::vec3 m_scale = { 1.f, 1.f, 1.f };
	gfx::TesselationMesh m_mesh;
	gfx::Texture m_heightmap;
	gfx::Texture m_normalmap;
	std::unordered_map<std::string, const gfx::Texture*> m_materials;
private:
	util::Image<uint8_t> m_border_map;
	gfx::Texture m_border_texture;
	float m_border_mix = 0.f;
private:
	util::Image<uint8_t> m_political_map;
	gfx::Texture m_political_texture;
	float m_political_mix = 0.f;
private:
	util::Image<uint8_t> m_political_boundaries;
	gfx::Texture m_political_boundaries_raw;
	gfx::Texture m_political_boundaries_blurred; // blurred version for smooth visuals
private:
	BoardMarker m_marker = {};
	bool m_marker_visible = false;
};

struct TilePaintJob {
	uint32_t tile;
	glm::vec3 color;
};

struct BorderPaintJob {
	uint32_t border;
	uint8_t color;
};

class Board {
public:
	Board(std::shared_ptr<gfx::Shader> tilemap, std::shared_ptr<gfx::Shader> blur_shader);
public:
	const glm::vec3 SCALE = { 1024.f, 32.f, 1024.f };
public:
	void generate(int seed);
	void reload();
	void update();
	void paint_tile(uint32_t tile, const glm::vec3 &color);
	void paint_border(uint32_t border, uint8_t color);
public:
	void set_marker(const BoardMarker &marker);
	void hide_marker();
	void set_border_mix(float mix);
public:
	fysx::HeightField* height_field();
	const util::Navigation& navigation() const;
	const Atlas& atlas() const { return m_atlas; }
	Atlas& atlas() { return m_atlas; }
	const Tile* tile_at(const glm::vec2 &position) const;
	glm::vec2 tile_center(uint32_t index) const;
	void find_path(const glm::vec2 &start, const glm::vec2 &end, std::list<glm::vec2> &path) const;
public:
	template <class Archive>
	void save(Archive &archive) const
	{
		archive(m_atlas, m_land_navigation);
	}
public:
	template <class Archive>
	void load(Archive &archive)
	{
		archive(m_atlas, m_land_navigation);
	}
public:
	void display(const util::Camera &camera);
	void display_wireframe(const util::Camera &camera);
private:
	Atlas m_atlas;
	BoardModel m_model;
	std::unique_ptr<fysx::HeightField> m_height_field;
	util::Navigation m_land_navigation;
private:
	std::queue<TilePaintJob> m_paint_jobs;
	std::queue<BorderPaintJob> m_border_paint_jobs;
private:
	void build_navigation();
};
