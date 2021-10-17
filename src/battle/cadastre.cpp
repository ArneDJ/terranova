#include <iostream>
#include <random>
#include <queue>
#include <algorithm>
#include <unordered_map>
#include <list>
#include <map>
#include <functional>
#include <math.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/voronoi.h"
#include "cadastre.h"

namespace carto {

struct ChainSegment {
	std::list<glm::vec2>::iterator left;
	std::list<glm::vec2>::iterator right;
	float distance;
};

struct ChainSplit {
	std::list<glm::vec2>::iterator target;
	std::list<glm::vec2>::iterator a;
	std::list<glm::vec2>::iterator b;
	glm::vec2 point;
	float arealeft;
	float arearight;
};

struct HamNode {
	int index;
	HamNode *parent = nullptr;
	std::vector<HamNode*> children;
};

static const float CELL_PLACE_SCALE = 0.75F;
static const size_t MAX_CELLS = 128;
static const size_t MIN_GATEWAY_DISTANCE = 5;
static const float PARCEL_DENSITY_DISTANCE = 25.F;

static void divide_polygon(std::list<glm::vec2> start, District *cell);
static float polygon_area(std::vector<glm::vec2> &vertices);
static ChainSplit find_chainsplit(std::list<glm::vec2> &polygon, std::list<glm::vec2>::iterator &longesta, std::list<glm::vec2>::iterator &longestb);

static glm::vec2 polygon_centroid(const std::vector<glm::vec2> &vertices)
{
	glm::vec2 centroid = { 0.f, 0.f };
	float area = 0.f;
	float x0 = 0.f; // Current vertex X
	float y0 = 0.f; // Current vertex Y
	float x1 = 0.f; // Next vertex X
	float y1 = 0.f; // Next vertex Y
	float a = 0.f;  // Partial signed area

	// For all vertices except last
	int i = 0;
	for (i = 0; i < vertices.size()-1; ++i) {
		x0 = vertices[i].x;
		y0 = vertices[i].y;
		x1 = vertices[i+1].x;
		y1 = vertices[i+1].y;
		a = x0*y1 - x1*y0;
		area += a;
		centroid.x += (x0 + x1)*a;
		centroid.y += (y0 + y1)*a;
	}

	// Do last vertex separately to avoid performing an expensive
	// modulus operation in each iteration.
	x0 = vertices[i].x;
	y0 = vertices[i].y;
	x1 = vertices[0].x;
	y1 = vertices[0].y;
	a = x0*y1 - x1*y0;
	area += a;
	centroid.x += (x0 + x1)*a;
	centroid.y += (y0 + y1)*a;

	area *= 0.5f;
	centroid.x /= (6.f*area);
	centroid.y /= (6.f*area);

	return centroid;
}

static WallSegment make_wallsegment(const District *left, const District *right)
{
	WallSegment wall;
	wall.left = left;
	wall.right = right;
	wall.S = {left->center, right->center};

	return wall;
}

static bool winding(Junction *a, Junction *b, glm::vec2 center)
{
	glm::vec2 av = glm::normalize(a->position - center);
	glm::vec2 ab = glm::normalize(b->position - center);

	return atan2(av.y, av.x) < atan2(ab.y, ab.x);
}

static bool comp_area(const Section *a, const Section*b)
{
	return a->area > b->area;
}

static bool comp_distance(const Section *a, const Section*b)
{
	return glm::distance(a->d0->center, a->d1->center) > glm::distance(b->d0->center, b->d1->center);
}

static bool clockwise_polygon(std::list<glm::vec2> &polygon)
{
	float sum = 0.f;

	std::list<glm::vec2>::iterator it;
	for (it = polygon.begin(); it != polygon.end(); it++){
		std::list<glm::vec2>::iterator next = std::next(it);
		if (next == polygon.end()) { next = polygon.begin(); }
		glm::vec2 a = *it;
		glm::vec2 b = *next;
		sum += (b.x - a.x) * (b.y + a.y);
	}

	return (sum > 0.f);
}

void Cadastre::generate(int seedling, uint32_t tile, geom::Rectangle bounds, size_t wall_radius)
{
	walls.clear();
	highways.clear();
	districts.clear();
	junctions.clear();
	sections.clear();

	seed = seedling;
	area = bounds;

	make_diagram(tile);

	mark_districts();
	mark_junctions();

	outline_walls(wall_radius);
	make_gateways();
	finalize_walls();
	make_highways();

	divide_parcels(wall_radius);
}

void Cadastre::make_diagram(uint32_t tile)
{
	std::mt19937 gen(seed);
	gen.discard(tile * MAX_CELLS * 2); // discard values with offset based on tile index so each site will have a different layout based on the same seed
	std::uniform_real_distribution<float> dist_x(area.min.x+(area.max.x-CELL_PLACE_SCALE*area.max.x), CELL_PLACE_SCALE*area.max.x);
	std::uniform_real_distribution<float> dist_y(area.min.y+(area.max.y-CELL_PLACE_SCALE*area.max.y), CELL_PLACE_SCALE*area.max.y);

	std::vector<glm::vec2> locations;
	for (int i = 0; i < MAX_CELLS; i++) {
		locations.push_back(glm::vec2(dist_x(gen), dist_y(gen)));
	}

	geom::VoronoiGraph voronoi;
	voronoi.generate(locations, area, 1);

	districts.resize(voronoi.cells.size());
	junctions.resize(voronoi.vertices.size());
	sections.resize(voronoi.edges.size());

	// adopt cell structures
	for (const auto &cell : voronoi.cells) {
		std::vector<District*> dneighbors;
		for (const auto &neighbor : cell.neighbors) {
			dneighbors.push_back(&districts[neighbor->index]);
		}
		std::vector<Junction*> djunctions;
		for (const auto &vertex : cell.vertices) {
			djunctions.push_back(&junctions[vertex->index]);
		}
		std::vector<Section*> dsections;
		for (const auto &edge : cell.edges) {
			dsections.push_back(&sections[edge->index]);
		}

		District d;
		d.index = cell.index;
		d.center = cell.center;
		d.neighbors = dneighbors;
		d.junctions = djunctions;
		d.sections = dsections;
		d.border = false;
		d.radius = 0;
		d.area = 0.f;
		d.tower = false;
		d.centroid = { 0.f, 0.f };

		districts[cell.index] = d;
	}

	// adapt vertex structures
	for (const auto &vertex : voronoi.vertices) {
		std::vector<Junction*> adjacent;
		for (const auto &neighbor : vertex.adjacent) {
			adjacent.push_back(&junctions[neighbor->index]);
		}
		std::vector<District*> touches;
		for (const auto &cell : vertex.cells) {
			touches.push_back(&districts[cell->index]);
		}

		Junction c;
		c.index = vertex.index;
		c.position = vertex.position;
		c.adjacent = adjacent;
		c.districts = touches;
		c.border = false;
		c.street = false;

		junctions[vertex.index] = c;
	}

	// adapt edge structures
	for (const auto &edge : voronoi.edges) {
		int index = edge.index;
		sections[index].j0 = &junctions[edge.left_vertex->index];
		sections[index].j1 = &junctions[edge.right_vertex->index];
		sections[index].border = false;
		sections[index].wall = false;
		sections[index].area = 0.f;
		sections[index].gateway = false;
		if (edge.left_cell != nullptr) {
			sections[index].d0 = &districts[edge.left_cell->index];
		} else {
			sections[index].d0 = &districts[edge.right_cell->index];
			sections[index].d0->border = true;
			sections[index].border = true;
			sections[index].j0->border = true;
			sections[index].j1->border = true;
		}
		if (edge.right_cell != nullptr) {
			sections[index].d1 = &districts[edge.right_cell->index];
		} else {
			sections[index].d1 = &districts[edge.left_cell->index];
			sections[index].d1->border = true;
			sections[index].border = true;
			sections[index].j0->border = true;
			sections[index].j1->border = true;
		}
	}

	for (auto &d : districts) {
		d. area = 0.f;
		for (const auto s : d.sections) {
			d.area += geom::triangle_area(d.center, s->j0->position, s->j1->position);
		}
		// sort the polygon vertices in order
		// ugly syntax
		std::sort(d.junctions.begin(), d.junctions.end(), std::bind(winding, std::placeholders::_1, std::placeholders::_2, d.center));
		// calculate centroid
		std::vector<glm::vec2> vertices;
		for (const auto j : d.junctions) {
			vertices.push_back(j->position);
		}
		d.centroid = polygon_centroid(vertices);
	}
}

void Cadastre::mark_districts(void)
{
	// find the center tile
	std::unordered_map<int, bool> visited;
	float min = glm::distance(area.min, area.max);
	glm::vec2 center = 0.5f * area.max;

	for (auto &d : districts) {
		visited[d.index] = false;
		d.radius = 0;
		float dist = glm::distance(center, d.center);
		if (dist < min) {
			min = dist;
			core = &d;
		}
	}

	// distance to center tile in graph
	std::queue<const District*> queue;
	queue.push(core);
	visited[core->index] = true;
	while (!queue.empty()) {
		const District *node = queue.front();
		queue.pop();
		int depth = node->radius + 1;
		for (auto neighbor : node->neighbors) {
			if (visited[neighbor->index] == false) {
				visited[neighbor->index] = true;
				neighbor->radius = depth;
				queue.push(neighbor);
			}
		}
	}
}

void Cadastre::mark_junctions(void)
{
	std::unordered_map<int, bool> visited;
	for (auto &j : junctions) {
		visited[j.index] = false;
		j.radius = 0;
	}
	for (const auto &root : core->junctions) {
		visited[root->index] = true;
	}

	// town center
	for (const auto &root : core->junctions) {
		std::queue<const Junction*> queue;
		queue.push(root);
		while (!queue.empty()) {
			const Junction *node = queue.front();
			queue.pop();
			int layer = node->radius + 1;
			for (auto neighbor : node->adjacent) {
				if (visited[neighbor->index] == false) {
					visited[neighbor->index] = true;
					neighbor->radius = layer;
					queue.push(neighbor);
				} else if (neighbor->radius > layer) {
					neighbor->radius = layer;
					queue.push(neighbor);
				}
			}
		}
	}
}

static HamNode *insert(HamNode *parent, int index)
{
	HamNode *node = new HamNode;
	node->parent =  parent;
	node->index = index;

	return node;
}

void Cadastre::outline_walls(size_t radius)
{
	// City wall is a Hamiltonian path
	std::map<std::pair<int, int>, bool> link; // two connected cells
	for (auto &sect : sections) {
		link[std::minmax(sect.d0->index, sect.d1->index)] = false;
	}

	size_t maxnodes = 0;
	std::vector<HamNode*> nodes;
	HamNode *start = nullptr;
	HamNode *end = nullptr;
	for (auto &d : districts) {
		if (d.radius == radius) {
			start = insert(nullptr, d.index);
			nodes.push_back(start);
			start->parent = start;
			std::queue<HamNode*> queue;
			queue.push(start);
			while (!queue.empty()) {
				HamNode *node = queue.front();
				queue.pop();
				const District *dist = &districts[node->index];
				HamNode *finish = nullptr;
				for (const auto neighbor : dist->neighbors) {
					if (neighbor->index == start->index) {
						// found a cyclic path
						finish = node;
					} else if (neighbor->radius == radius) {
						// backtrack through the nodes and check if it has already been visited
						bool visited = false;
						HamNode *prev = node;
						while (prev != start) {
							if (prev->index == neighbor->index) {
								visited = true;
								break;
							}
							prev = prev->parent;
						}
						// if the node hasn't been visited add is at a child
						if (visited == false) {
							HamNode *child = insert(node, neighbor->index);
							nodes.push_back(child);
							node->children.push_back(child);
							queue.push(child);
						}
					}
				}
				if (finish != nullptr) { // found a valid cyclic path
					size_t nnodes = 0;
					HamNode *node = finish;
					while (node != start) {
						node = node->parent;
						nnodes++;
					}
					// largest cyclic path will be chosen
					if (nnodes > maxnodes) {
						maxnodes = nnodes;
						end = finish;
					}
				}
			}
			break;
		}
	}
	if (start != nullptr && end != nullptr) {
		HamNode *node = end;
		while (node != start) {
			HamNode *parent = node->parent;
			auto wall = make_wallsegment(&districts[node->index], &districts[parent->index]);
			walls.push_back(wall);
			link[std::minmax(node->index, parent->index)] = true;
			node = parent;
		}
		auto wall = make_wallsegment(&districts[start->index], &districts[end->index]);
		walls.push_back(wall);
		link[std::minmax(start->index, end->index)] = true;
	}

	for (int i = 0; i < nodes.size(); i++) { delete nodes[i]; }

	for (auto &sect : sections) {
		sect.wall = link[std::minmax(sect.d0->index, sect.d1->index)];
	}

	for (auto &sect : sections) {
		sect.area = 0.f;
		if (sect.wall) {
			glm::vec2 outward = geom::midpoint(sect.d0->center, sect.d1->center);
			glm::vec2 right = sect.d0->center - outward;
			glm::vec2 a = sect.j0->position - right;;
			glm::vec2 b = sect.j0->position + right;;
			glm::vec2 c = sect.j1->position - right;;
			glm::vec2 d = sect.j1->position + right;;
			sect.area += geom::triangle_area(a, c, d);
			sect.area += geom::triangle_area(a, d, b);
		}
	}
}

void Cadastre::make_gateways(void)
{
	std::vector<Section*> candidates;
	for (auto &sect : sections) {
		if (sect.wall) {
			candidates.push_back(&sect);
		}
	}
	// traverse per district
	std::unordered_map<int, bool> reserved;
	std::unordered_map<int, int> depth;
	for (const auto &dist : districts) {
		reserved[dist.index] = false;
		depth[dist.index] = 0;
	}
	// sort gate candidates by wall distance
	std::sort(candidates.begin(), candidates.end(), comp_distance);
	for (auto sect : candidates) {
		if (reserved[sect->d0->index] == false && reserved[sect->d1->index] == false) {
			sect->gateway = true;
			std::queue<District*> queue;
			queue.push(sect->d0);
			queue.push(sect->d1);
			while (!queue.empty()) {
				District *node = queue.front();
				queue.pop();
				int layer = depth[node->index] + 1;
				reserved[node->index] = true;
				for (auto neighbor : node->neighbors) {
					if (reserved[neighbor->index] == false && layer < MIN_GATEWAY_DISTANCE) {
						depth[neighbor->index] = layer;
						queue.push(neighbor);
					}
				}
			}
		}
	}
}
	
void Cadastre::finalize_walls(void)
{
	std::map<std::pair<int, int>, bool> link; // two connected cells
	for (auto &sect : sections) {
		link[std::minmax(sect.d0->index, sect.d1->index)] = sect.gateway;
	}
	for (auto &wall : walls) {
		wall.gate = link[std::minmax(wall.left->index, wall.right->index)];
	}
	// correct polygon winding
	std::list<glm::vec2> polygon;
	for (auto &wall : walls) { polygon.push_back(wall.S.A); }
	if (clockwise_polygon(polygon)) {
		for (auto &wall : walls) {
			std::swap(wall.S.A, wall.S.B);
			std::swap(wall.left, wall.right);
		}
	}
}

void Cadastre::make_highways(void)
{
	// highway from gateway to town core
	for (auto &sect : sections) {
		if (sect.gateway) {
			geom::Segment S = { sect.j0->position, sect.j1->position };
			highways.push_back(S);
			Junction *start = sect.j0->radius < sect.j1->radius ? sect.j0 : sect.j1;
			std::queue<Junction*> queue;
			queue.push(start);
			while (!queue.empty()) {
				Junction *node = queue.front();
				queue.pop();
				for (auto neighbor : node->adjacent) {
					if (neighbor->radius < node->radius) {
						queue.push(neighbor);
						geom::Segment highway = { node->position, neighbor->position };
						highways.push_back(highway);
						break;
					}
				}
			}
		}
	}

	// outside town center
	std::unordered_map<int, bool> visited;
	std::unordered_map<int, int> depth;
	for (const auto &j : junctions) {
		visited[j.index] = j.border;
		depth[j.index] = 0;
	}

	for (const auto &root : junctions) {
		if (root.border == true) {
			std::queue<const Junction*> queue;
			queue.push(&root);
			while (!queue.empty()) {
				const Junction *node = queue.front();
				queue.pop();
				int layer = depth[node->index] + 1;
				for (auto neighbor : node->adjacent) {
					if (visited[neighbor->index] == false) {
						visited[neighbor->index] = true;
						depth[neighbor->index] = layer;
						queue.push(neighbor);
					} else if (depth[neighbor->index] > layer) {
						depth[neighbor->index] = layer;
						queue.push(neighbor);
					}
				}
			}
		}
	}
	for (auto &sect : sections) {
		if (sect.gateway) {
			Junction *outward = sect.j0->radius > sect.j1->radius ? sect.j0 : sect.j1;
			Junction *inward = sect.j0->radius < sect.j1->radius ? sect.j0 : sect.j1;
			std::queue<Junction*> queue;
			queue.push(outward);
			glm::vec2 dir = glm::normalize(outward->position - inward->position);
			while (!queue.empty()) {
				Junction *node = queue.front();
				queue.pop();
				Junction *next = nullptr;
				float maxdot = -1.f;
				for (auto neighbor : node->adjacent) {
					if (depth[neighbor->index] < depth[node->index]) {
						glm::vec2 nextdir = glm::normalize(neighbor->position - node->position);
						float dotp = glm::dot(nextdir, dir);
						if (dotp > maxdot) {
							maxdot = dotp;
							next = neighbor;
						}
					}
				}
				if (next != nullptr) {
					queue.push(next);
					geom::Segment S = { node->position, next->position };
					highways.push_back(S);
					node->street = true;
					next->street = true;
				}
			}
		}
	}
}

void Cadastre::divide_parcels(size_t radius)
{
	for (auto &d : districts) {
		if (d.radius < radius+1) {
			std::list<glm::vec2> polygon;
			for (auto jun : d.junctions) {
				// scale the polygon so the houses don't overlap with the street
				glm::vec2 p = jun->position - d.center;
				glm::vec2 scaled = 0.9F * p;
				polygon.push_back(d.center+scaled);
			}
			// set density
			for (std::list<glm::vec2>::iterator it = polygon.begin(); it != polygon.end();) {
				std::list<glm::vec2>::iterator next = std::next(it);
				if (next == polygon.end()) {
					next = polygon.begin();
				}
				glm::vec2 a = *it;
				glm::vec2 b = *next;
				if (glm::distance(a, b) > PARCEL_DENSITY_DISTANCE) {
					polygon.insert(next, geom::midpoint(a, b));
				}
				if (next != polygon.begin()) {
					it = next;
				} else {
					it = polygon.end();
				}
			}
			divide_polygon(polygon, &d);
		}
	}
}

static Parcel make_parcel(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d)
{
	Parcel parc;
	parc.quad = {a, b, c, d};

	parc.centroid = geom::quadrilateral_centroid(parc.quad);
	parc.direction = glm::normalize(geom::midpoint(a, b) - parc.centroid);

	return parc;
}

static bool longest_segment(ChainSegment &a, ChainSegment &b)
{
	return a.distance > b.distance;
}

static void divide_polygon(std::list<glm::vec2> start, District *cell)
{
	std::queue<std::list<glm::vec2>> queue; // queue of polygons to split
	queue.push(start);

	while (!queue.empty()) {
		std::list<glm::vec2> polygon = queue.front();
		queue.pop();
		if (polygon.size() == 4) {
			glm::vec2 a = polygon.front(); polygon.pop_front();
			glm::vec2 b = polygon.front(); polygon.pop_front();
			glm::vec2 c= polygon.front(); polygon.pop_front();
			glm::vec2 d = polygon.front(); polygon.pop_front();
			auto parc = make_parcel(c, d, a, b);
			if (geom::convex_quadrilateral(parc.quad)) {
				cell->parcels.push_back(parc);
			}
		} else if (polygon.size() > 4) {
			// remove duplicates
			for (std::list<glm::vec2>::iterator it = polygon.begin(); it != polygon.end(); ++it) {
				std::list<glm::vec2>::iterator next = std::next(it);
				if (next == polygon.end()) {
					next = polygon.begin();
				}
				glm::vec2 a = *it;
				glm::vec2 b = *next;
				if (a.x == b.x && a.y == b.y) {
					polygon.erase(next);
				}
			}
			// of all the polygon segments find a point that lies on a perpendicular line to the segment that divides the polygon in two "almost equal" areas
			// sort from longest to shortest segment
			std::vector<ChainSegment> chainseg;
			for (std::list<glm::vec2>::iterator it = polygon.begin(); it != polygon.end(); ++it) {
				std::list<glm::vec2>::iterator next = std::next(it);
				if (next == polygon.end()) { next = polygon.begin(); }
				ChainSegment seg = { it, next, glm::distance(*it, *next) };
				chainseg.push_back(seg);
			}
			std::sort(chainseg.begin(), chainseg.end(), longest_segment);

			for (auto &splitter : chainseg) {
				ChainSplit split = find_chainsplit(polygon, splitter.left, splitter.right);
				// divide the polygon in two
				const std::list<glm::vec2>::iterator splitstart = polygon.insert(split.b, split.point);
				// first half
				std::list<glm::vec2> right;
				std::list<glm::vec2>::iterator it = splitstart;
				while (it != split.target) {
					right.push_back(*it);
					it++;
					if (it == polygon.end()) { it = polygon.begin(); }
				}
				right.push_back(*split.target);
				// second half
				std::list<glm::vec2> left;
				it = splitstart;
				while (it != split.target) {
					left.push_back(*it);
					if (it == polygon.begin()) {
						it = std::prev(polygon.end());
					} else {
						it--;
					}
				}
				left.push_back(*split.target);

				std::vector<glm::vec2> rightpoints;
				std::vector<glm::vec2> leftpoints;
				for (auto point : right) {
					rightpoints.push_back(point);
				}
				for (auto point : left) {
					leftpoints.push_back(point);
				}
				float arearight = polygon_area(rightpoints);
				float arealeft = polygon_area(leftpoints);

				// best polygon split was found
				// add the two halves of the polygon to the queue
				if (arearight > 1.f && arealeft > 1.f) {
					queue.push(right);
					queue.push(left);
					break;
				}
				// remove the inserted point from the original polygon
				polygon.erase(splitstart);
			}
		}
	}
}

// ugly
static ChainSplit find_chainsplit(std::list<glm::vec2> &polygon, std::list<glm::vec2>::iterator &longesta, std::list<glm::vec2>::iterator &longestb)
{
	ChainSplit split;

	split.a = longesta;
	split.b = longestb;

	// find the best possible way to split the polygon in two roughly equal areas
	float min = std::numeric_limits<float>::max();
	for (std::list<glm::vec2>::iterator it = polygon.begin(); it != polygon.end(); ++it) {
		if (it != split.a && it != split.b) {
			glm::vec2 point = *it;
			// now add the new vertex to the list
			const glm::vec2 splitpoint = geom::closest_point_segment(point, *split.a, *split.b);
			const std::list<glm::vec2>::iterator splitstart = polygon.insert(split.b, splitpoint);
			// now split the polygon in two
			// first half
			std::list<glm::vec2> right;
			std::list<glm::vec2>::iterator chain = splitstart;
			while (chain != it) {
				right.push_back(*chain);
				chain++;
				if (chain == polygon.end()) {
					chain = polygon.begin();
				}
			}
			right.push_back(*it);
			// second half
			std::list<glm::vec2> left;
			chain = splitstart;
			while (chain != it) {
				left.push_back(*chain);
				if (chain == polygon.begin()) {
					chain = std::prev(polygon.end());
				} else {
					chain--;
				}
			}
			left.push_back(*it);

			std::vector<glm::vec2> rightpoints;
			std::vector<glm::vec2> leftpoints;
			for (auto point : right) {
				rightpoints.push_back(point);
			}
			for (auto point : left) {
				leftpoints.push_back(point);
			}
			split.arearight = polygon_area(rightpoints);
			if (split.arearight == 0.F) {
				split.arearight =  std::numeric_limits<float>::epsilon();
			}
			split.arealeft = polygon_area(leftpoints);
			if (split.arealeft == 0.F) {
				split.arealeft =  std::numeric_limits<float>::epsilon();
			}

			float ratio = split.arearight / split.arealeft;

			// best ratio is the closest to 1
			float dist = fabs(1.F - ratio);
			if (dist < min) {
				min = dist;
				split.target = it;
				split.point = splitpoint;
			}

			// remove the inserted point from the original polygon
			polygon.erase(splitstart);
		}
	}

	return split;
}

static float polygon_area(std::vector<glm::vec2> &vertices)
{
	// triangle
	if (vertices.size() == 3) {
		return geom::triangle_area(vertices[0], vertices[1], vertices[2]);
	}

	float area = 0.f;

	// Calculate value of shoelace formula
	int j = vertices.size() - 1;
	for (int i = 0; i < vertices.size(); i++) {
		area += (vertices[j].x + vertices[i].x) * (vertices[j].y - vertices[i].y);
		j = i;
	}

	return fabs(area / 2.f);
}

};
