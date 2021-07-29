
class WorldModel {
public:
	void reload(const geom::VoronoiGraph &graph, int seed);
public:
	void display() const;
private:
	gfx::Mesh m_mesh;
	std::vector<gfx::Vertex> m_vertices;
	std::vector<uint32_t> m_indices;
};

class WorldMap {
public:
	const geom::Rectangle BOUNDS = { { 0.F, 0.F }, { 128.F, 128.F } };
public:
	void generate(int seed);
	void prepare();
public:
	template <class Archive>
	void save(Archive &archive) const 
	{
		archive(m_seed, m_graph);
	}
public:
	template <class Archive>
	void load(Archive &archive)
	{
		archive(m_seed, m_graph);
	}
public:
	void display();
private:
	int m_seed;
	geom::VoronoiGraph m_graph;
	WorldModel m_model;
};
