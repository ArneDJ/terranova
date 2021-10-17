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

class Landscaper {
public:
	geom::Rectangle bounds = {};
	std::unordered_map<uint32_t, LandscapeObject> houses; // key: mold_id
public:
	void clear();
	void generate(int seed, uint32_t tile, uint8_t town_size);
public:
	void add_house(uint32_t mold_id, const geom::AABB &bounds);
private:
	Cadastre m_cadastre;
private:
	void spawn_houses(bool walled, uint8_t town_size);
};

};
