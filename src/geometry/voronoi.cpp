#include <vector>
#include <set>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "geometry.h"

#define JC_VORONOI_IMPLEMENTATION
#define JCV_REAL_TYPE double
#define JCV_ATAN2 atan2
#define JCV_SQRT sqrt
#define JCV_FLT_MAX DBL_MAX
#define JCV_PI 3.141592653589793115997963468544185161590576171875
#include "../extern/jcvoronoi/jc_voronoi.h"

#include "../util/serialize.h"

#include "voronoi.h"

namespace geom {
	
static void relax_points(const jcv_diagram *diagram, std::vector<jcv_point> &points);
static void adapt_cells(const jcv_diagram *diagram, std::vector<VoronoiCell> &cells);
static void adapt_vertices(const jcv_diagram *diagram, std::vector<VoronoiVertex> &vertices);
static void pair_duality(const jcv_diagram *diagram, std::vector<VoronoiCell> &cells, std::vector<VoronoiVertex> &vertices);
static void adapt_edges(const jcv_diagram *diagram, std::vector<VoronoiCell> &cells, std::vector<VoronoiVertex> &vertices, std::vector<VoronoiEdge> &edges);

void VoronoiGraph::generate(const std::vector<glm::vec2> &locations, const Bounding<glm::vec2> &bounds, uint8_t relaxations)
{
	clear();

	// import points
	std::vector<jcv_point> points;
	for (const auto &location : locations) {
		jcv_point point = { location.x, location.y };
		points.push_back(point);
	}

	// diagram bounds
	const jcv_rect rect = {
		{ bounds.min.x, bounds.min.y},
		{ bounds.max.x, bounds.max.y},
	};

	// generate the diagram
	jcv_diagram diagram;
	memset(&diagram, 0, sizeof(jcv_diagram));
	jcv_diagram_generate(points.size(), points.data(), &rect, 0, &diagram);

	// relax the diagram if requested
	for (uint8_t i = 0; i < relaxations; i++) {
		std::vector<jcv_point> relaxed_points;
		relax_points(&diagram, relaxed_points);
		jcv_diagram_generate(relaxed_points.size(), relaxed_points.data(), &rect, 0, &diagram);
	}

	// vertex data
	jcv_diagram_generate_vertices(&diagram);

	adapt_cells(&diagram, cells);

	adapt_vertices(&diagram, vertices);

	pair_duality(&diagram, cells, vertices);

	adapt_edges(&diagram, cells, vertices, edges);

	jcv_diagram_free(&diagram);

	// create data to serialize graph

	for (const auto &edge : edges) {
		auto cell_connection = std::make_pair(edge.left_cell->index, edge.right_cell->index);
		auto vertex_connection = std::make_pair(edge.left_vertex->index, edge.right_vertex->index);
		m_connected_cells[edge.index] = cell_connection;
		m_connected_vertices[edge.index] = vertex_connection;
	}
	
	for (const auto &cell : cells) {
		for (const auto &vertex : cell.vertices) {
			auto connection = std::make_pair(cell.index, vertex->index);
			m_cell_vertex_connections.push_back(connection);
		}
	}
}
	
void VoronoiGraph::clear()
{
	cells.clear();
	vertices.clear();
	edges.clear();

	m_connected_cells.clear();
	m_connected_vertices.clear();
	m_cell_vertex_connections.clear();
}
	
void VoronoiGraph::unserialize_nodes()
{
	// connect with pointers
	for (const auto &connection : m_connected_cells) {
		const auto &cell_pair = connection.second;
		VoronoiEdge *edge = &edges[connection.first];
		VoronoiCell *left_cell = &cells[cell_pair.first];
		VoronoiCell *right_cell = &cells[cell_pair.second];
		edge->left_cell = left_cell;
		edge->right_cell = right_cell;
		left_cell->edges.push_back(edge);
		left_cell->neighbors.push_back(right_cell);
		right_cell->edges.push_back(edge);
		right_cell->neighbors.push_back(left_cell);
	}
	// vertex connections
	for (const auto &connection : m_connected_vertices) {
		const auto &vertex_pair = connection.second;
		VoronoiEdge *edge = &edges[connection.first];
		VoronoiVertex *left_vertex = &vertices[vertex_pair.first];
		VoronoiVertex *right_vertex = &vertices[vertex_pair.second];
		edge->left_vertex = left_vertex;
		edge->right_vertex = right_vertex;
		left_vertex->adjacent.push_back(right_vertex);
		right_vertex->adjacent.push_back(left_vertex);
	}
	// vertex cell connections
	for (const auto &connection : m_cell_vertex_connections) {
		VoronoiCell *cell = &cells[connection.first];
		VoronoiVertex *vertex = &vertices[connection.second];
		cell->vertices.push_back(vertex);
		vertex->cells.push_back(cell);
	}
}

static void relax_points(const jcv_diagram *diagram, std::vector<jcv_point> &points)
{
	const jcv_site* sites = jcv_diagram_get_sites(diagram);
	for( int i = 0; i < diagram->numsites; ++i ) {
		const jcv_site* site = &sites[i];
		jcv_point sum = site->p;
		int count = 1;

		const jcv_graphedge* edge = site->edges;

		while (edge) {
			sum.x += edge->pos[0].x;
			sum.y += edge->pos[0].y;
			++count;
			edge = edge->next;
		}

		jcv_point point;
		point.x = sum.x / float(count);
		point.y = sum.y / float(count);
		points.push_back(point);
	}
}

static void adapt_cells(const jcv_diagram *diagram, std::vector<VoronoiCell> &cells)
{
	cells.resize(diagram->numsites);

	const jcv_site *sites = jcv_diagram_get_sites(diagram);

	// get each cell
	for (int i = 0; i < diagram->numsites; i++) {
		const jcv_site *site = &sites[i];
		VoronoiCell cell;
		jcv_point p = site->p;
		cell.center = glm::vec2(p.x, p.y);
		cell.index = site->index;
		cells[site->index] = cell;
	}
	// get neighbor cells
	for (int i = 0; i < diagram->numsites; i++) { 
		const jcv_site *site = &sites[i];
		const jcv_graphedge *edge = site->edges;
		while (edge) {
			const jcv_site *neighbor = edge->neighbor;
			if (neighbor != nullptr) {
				cells[site->index].neighbors.push_back(&cells[neighbor->index]);
			}

			edge = edge->next;
		}
	}
}

static void adapt_vertices(const jcv_diagram *diagram, std::vector<VoronoiVertex> &vertices)
{
	vertices.resize(diagram->internal->numvertices);

	const jcv_site *sites = jcv_diagram_get_sites(diagram);
	const jcv_vertex *vertex = jcv_diagram_get_vertices(diagram);
	while (vertex) {
		VoronoiVertex v;
		v.index = vertex->index;
		v.position = glm::vec2(vertex->pos.x, vertex->pos.y);
		jcv_vertex_edge *edges = vertex->edges;
		while (edges) {
			jcv_vertex *neighbor = edges->neighbor;
			v.adjacent.push_back(&vertices[neighbor->index]);
			edges = edges->next;
		}
		vertices[vertex->index] = v;
		vertex = jcv_diagram_get_next_vertex(vertex);
	}
}

static void pair_duality(const jcv_diagram *diagram, std::vector<VoronoiCell> &cells, std::vector<VoronoiVertex> &vertices)
{
	const jcv_site *sites = jcv_diagram_get_sites(diagram);

	// get vertex and cell duality
	for (int i = 0; i < diagram->numsites; i++) {
		const jcv_site *site = &sites[i];
		const jcv_graphedge *edge = site->edges;
		std::set<uint32_t> indices;
		while (edge) {
			const jcv_altered_edge *altered = get_altered_edge(edge);
			jcv_vertex *a = altered->vertices[0];
			indices.insert(a->index);
			jcv_vertex *b = altered->vertices[1];
			indices.insert(b->index);

			edge = edge->next;
		}
		for (auto index : indices) {
			cells[site->index].vertices.push_back(&vertices[index]);
		}
	}

	// add duality
	for (auto &cell : cells) {
		for (auto &vertex : cell.vertices) {
			vertices[vertex->index].cells.push_back(&cells[cell.index]);
		}
	}
}

static void adapt_edges(const jcv_diagram *diagram, std::vector<VoronoiCell> &cells, std::vector<VoronoiVertex> &vertices, std::vector<VoronoiEdge> &edges)
{
	const jcv_edge *jcedge = jcv_diagram_get_edges(diagram);
	while (jcedge) {
		VoronoiEdge e;
		if (jcedge->sites[0] != nullptr) {
			e.left_cell = &cells[jcedge->sites[0]->index];
			// literally an edge case
			if (jcedge->sites[1] == nullptr) {
				e.right_cell = e.left_cell;
			}
		}
		if (jcedge->sites[1] != nullptr) {
			e.right_cell = &cells[jcedge->sites[1]->index];
			// literally an edge case
			if (jcedge->sites[0] == nullptr) {
				e.left_cell = e.right_cell;
			}
		}

    		const jcv_altered_edge *alter = (jcv_altered_edge*)jcedge;
		e.left_vertex = &vertices[alter->vertices[0]->index];
		e.right_vertex = &vertices[alter->vertices[1]->index];

		edges.push_back(e);
		jcedge = jcv_diagram_get_next_edge(jcedge);
	}

	for (int i = 0; i < edges.size(); i++) {
		VoronoiEdge *e = &edges[i];
		e->index = i;
		if (e->left_cell != nullptr) {
			cells[e->left_cell->index].edges.push_back(&edges[i]);
		}
		if (e->right_cell != nullptr) {
			cells[e->right_cell->index].edges.push_back(&edges[i]);
		}
	}
}

};
