#pragma once
#include <map>

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

template <class Archive>
void serialize(Archive &archive, VoronoiEdge &edge)
{
	archive(edge.index);
}

template <class Archive>
void serialize(Archive &archive, VoronoiVertex &vertex)
{
	archive(vertex.index, vertex.position);
}

template <class Archive>
void serialize(Archive &archive, VoronoiCell &cell)
{
	archive(cell.index, cell.center);
}

// contains voronoi cells overlapping an area defined by bounds
// for spatial searching
struct VoronoiCellsRegion {
	Rectangle bounds = {};
	std::vector<const VoronoiCell*> cells;
};

// search result of spatial searching
struct VoronoiSearchResult {
	bool found = false;
	const VoronoiCell *cell;
};

class VoronoiGraph {
public:
	std::vector<VoronoiCell> cells;
	std::vector<VoronoiVertex> vertices;
	std::vector<VoronoiEdge> edges;
public:
	void generate(const std::vector<glm::vec2> &locations, const Bounding<glm::vec2> &bounds, uint8_t relaxations = 0);
	void clear();
public:
	VoronoiSearchResult cell_at(const glm::vec2 &position) const;
public:
	template <class Archive>
	void save(Archive &archive) const
	{
		archive(
			m_bounds.min, m_bounds.max,
			cells, vertices, edges,
			m_connected_cells,
			m_connected_vertices,
			m_cell_vertex_connections
		);
	}
	template <class Archive>
	void load(Archive &archive)
	{
		// clear data just to be sure
		clear();

		archive(
			m_bounds.min, m_bounds.max,
			cells, vertices, edges,
			m_connected_cells,
			m_connected_vertices,
			m_cell_vertex_connections
		);

		unserialize_nodes();

		create_spatial_map();
	}
private:
	Bounding<glm::vec2> m_bounds = {};
	glm::vec2 m_region_scale = {};
	std::vector<VoronoiCellsRegion> m_spatial_map;
	// FIXME remove
	int n_outside = 0;
	int n_bounds_inside = 0;
	int n_overlap = 0;
	int n_center_inside = 0;
	int n_vertex_inside = 0;
	int n_triangle_inside = 0;
	int n_total_cases = 0;
	int n_total = 0;
private:
	// left: edge index
	// right: connected cells 
	std::map<uint32_t, std::pair<uint32_t, uint32_t>> m_connected_cells;
	// left: edge index
	// right: connected vertices 
	std::map<uint32_t, std::pair<uint32_t, uint32_t>> m_connected_vertices;
	// left: cell index
	// right: vertex index
	std::vector<std::pair<uint32_t, uint32_t>> m_cell_vertex_connections;
private:
	void unserialize_nodes();
	void create_spatial_map();
	void add_cell_to_regions(const VoronoiCell *cell);
	bool cell_overlaps_rectangle(const VoronoiCell *cell, const Rectangle &rectangle);
};

};
