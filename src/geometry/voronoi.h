namespace geom {

struct VoronoiCell;
struct VoronoiEdge;
struct VoronoiVertex;
	
struct VoronoiEdge {
	uint32_t index; // index in the linear array of edges
	const VoronoiVertex *left_vertex = nullptr;
	const VoronoiVertex *right_vertex = nullptr;
	const VoronoiCell *left_cell = nullptr;
	const VoronoiCell *right_cell = nullptr;
};

struct VoronoiVertex {
	uint32_t index; // index in the linear array of vertices
	glm::vec2 position = {};
	std::vector<const VoronoiVertex*> adjacent;
	std::vector<const VoronoiCell*> cells;
};

struct VoronoiCell {
	uint32_t index; // index in the linear array of cells
	glm::vec2 center = {};
	std::vector<const VoronoiCell*> neighbors;
	std::vector<const VoronoiVertex*> vertices;
	std::vector<const VoronoiEdge*> edges;
};

class VoronoiGraph {
public:
	std::vector<VoronoiCell> cells;
	std::vector<VoronoiVertex> vertices;
	std::vector<VoronoiEdge> edges;
public:
	void generate(const std::vector<glm::vec2> &locations, const Bounding<glm::vec2> &bounds, uint8_t relaxations = 0);
};

};
