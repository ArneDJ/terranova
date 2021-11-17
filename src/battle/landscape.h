namespace carto {

struct LandscapeObjectTransform {
	glm::vec2 position = {};
	float angle = 0.f;
};

struct LandscapeObject {
	uint32_t mold_id = 0; // id of the module type
	glm::vec3 bounds = {};
	std::vector<LandscapeObjectTransform> transforms;
};

// wall segments to make a town wall
struct LandscapeFortification {
	LandscapeObject wall_even;
	LandscapeObject wall_both;
	LandscapeObject wall_left;
	LandscapeObject wall_right;
	LandscapeObject ramp;
	LandscapeObject gate;
	LandscapeObject tower;
};

class Landscaper {
public:
	geom::Rectangle bounds = {};
	std::unordered_map<uint32_t, LandscapeObject> houses; // key: mold_id
	LandscapeFortification fortification;
public:
	void clear();
	void generate(int seed, uint32_t tile, uint8_t town_size, const util::Image<float> &heightmap);
public:
	void add_house(uint32_t mold_id, const geom::AABB &bounds);
private:
	Cadastre m_cadastre;
private:
	void spawn_houses(bool walled, uint8_t town_size);
	void spawn_walls(const util::Image<float> &heightmap);
	void fill_wall(const geom::Segment &segment, const util::Image<float> &heightmap, LandscapeFortification &output);
};

};
