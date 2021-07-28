
class WorldModel {
public:
	void reload(const geom::VoronoiGraph &graph);
public:
	void display() const;
private:
	gfx::Mesh m_mesh;
	std::vector<gfx::Vertex> m_vertices;
	std::vector<uint32_t> m_indices;
};

class WorldMap {
public:
	const geom::Rectangle BOUNDS = { { 0.F, 0.F }, { 64.F, 64.F } };
public:
	void generate(int seed);
	//void save();
	//void load();
public:
	void display();
private:
	geom::VoronoiGraph m_graph;
	WorldModel m_model;
};
