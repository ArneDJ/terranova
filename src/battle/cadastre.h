namespace carto {

struct District;
struct Junction;
struct Section;

struct Parcel {
	geom::Quadrilateral quad;
	glm::vec2 centroid;
	glm::vec2 direction; // normalized direction vector to the closest street, if a parcel has a building on it it will be rotated in that direction
};

struct WallSegment {
	const District *left = nullptr;
	const District *right = nullptr;
	geom::Segment S = {};
	bool gate = false;
};

struct Section {
	uint32_t index;
	Junction *j0 = nullptr;
	Junction *j1 = nullptr;
	District *d0 = nullptr;
	District *d1 = nullptr;
	bool border = false;
	bool wall = false; // a wall goes through here
	float area = 0.f;
	bool gateway = false;
};

struct Junction {
	uint32_t index;
	glm::vec2 position = {};
	std::vector<Junction*> adjacent;
	std::vector<District*> districts;
	bool border = false;
	int radius = 0;
	bool street = false;
};

struct District {
	uint32_t index;
	glm::vec2 center = {};
	std::vector<District*> neighbors;
	std::vector<Junction*> junctions;
	std::vector<Section*> sections;
	bool border = false;
	int radius = 0; // distance to center in graph structure
	float area = 0.f;
	bool tower = false;
	glm::vec2 centroid = {};
	std::vector<Parcel> parcels;
};

class Cadastre {
public:
	District *core;
	int seed;
	std::vector<WallSegment> walls;
	std::vector<geom::Segment> highways;
	geom::Rectangle area;
	// graph structures
	std::vector<District> districts;
	std::vector<Junction> junctions;
	std::vector<Section> sections;
public:
	void generate(int seedling, uint32_t tile, geom::Rectangle bounds, size_t wall_radius);
private:
	void make_diagram(uint32_t tile);
	void mark_districts(void);
	void mark_junctions(void);
	void outline_walls(size_t radius);
	void make_gateways(void);
	void finalize_walls(void);
	void make_highways(void);
	void divide_parcels(size_t radius);
};

};
