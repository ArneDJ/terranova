#include <vector>
#include <random>
#include <memory>
#include <queue>
#include <list>
#include <unordered_map>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/fastnoise/FastNoise.h"

#include "../extern/poisson/PoissonGenerator.h"

#include "../geometry/geometry.h"
#include "../geometry/voronoi.h"
#include "../geometry/transform.h"
#include "../util/image.h"

#include "atlas.h"
	
static RiverBranch *create_branch(const Corner *confluence)
{
	RiverBranch *node = new RiverBranch;
	node->confluence = confluence;
	node->left = nullptr;
	node->right = nullptr;
	node->streamorder = 1;

	return node;
}

static void prune_branches(RiverBranch *root)
{
	if (root == nullptr) { return; }

	std::queue<RiverBranch*> queue;
	queue.push(root);

	while (!queue.empty()) {
		RiverBranch *front = queue.front();
		queue.pop();
		if (front->left) { queue.push(front->left); }
		if (front->right) { queue.push(front->right); }

		delete front;
	}

	root = nullptr;
}

static void delete_basin(DrainageBasin *tree)
{
	if (tree->mouth == nullptr) { return; }

	prune_branches(tree->mouth);

	tree->mouth = nullptr;
}

// Strahler stream order
// https://en.wikipedia.org/wiki/Strahler_number
static inline int strahler(const RiverBranch *node)
{
	// if node has no children it is a leaf with stream order 1
	if (node->left == nullptr && node->right == nullptr) {
		return 1;
	}

	int left = (node->left != nullptr) ? node->left->streamorder : 0;
	int right = (node->right != nullptr) ? node->right->streamorder : 0;

	if (left == right) {
		return std::max(left, right) + 1;
	} else {
		return std::max(left, right);
	}
}

// Shreve stream order
// https://en.wikipedia.org/wiki/Stream_order#Shreve_stream_order
static inline int shreve(const RiverBranch *node)
{
	// if node has no children it is a leaf with stream order 1
	if (node->left == nullptr && node->right == nullptr) {
		return 1;
	}

	int left = (node->left != nullptr) ? node->left->streamorder : 0;
	int right = (node->right != nullptr) ? node->right->streamorder : 0;

	return left + right;
}

static inline int postorder_level(RiverBranch *node)
{
	if (node->left == nullptr && node->right == nullptr) {
		return 0;
	}

	if (node->left != nullptr && node->right != nullptr) {
		return std::max(node->left->depth, node->right->depth) + 1;
	}

	if (node->left) { return node->left->depth + 1; }
	if (node->right) { return node->right->depth + 1; }

	return 0;
}

// post order tree traversal
static void stream_postorder(DrainageBasin *tree)
{
	std::list<RiverBranch*> stack;
	RiverBranch *prev = nullptr;
	stack.push_back(tree->mouth);

	while (!stack.empty()) {
		RiverBranch *current = stack.back();

		if (prev == nullptr || prev->left == current || prev->right == current) {
			if (current->left != nullptr) {
				stack.push_back(current->left);
			} else if (current->right != nullptr) {
				stack.push_back(current->right);
			} else {
				current->streamorder = strahler(current);
    				current->depth = postorder_level(current);
				stack.pop_back();
			}
		} else if (current->left == prev) {
			if (current->right != nullptr) {
				stack.push_back(current->right);
			} else {
				current->streamorder = strahler(current);
    				current->depth = postorder_level(current);
				stack.pop_back();
			}
		} else if (current->right == prev) {
			current->streamorder = strahler(current);
    			current->depth = postorder_level(current);
			stack.pop_back();
		}

		prev = current;
	}
}

static bool prunable(const RiverBranch *node, uint8_t min_stream)
{
	// prune rivers right next to mountains
	/*
	for (const auto t : node->confluence->touches) {
		if (t->relief == ReliefType::HIGHLAND) { return true; }
	}
	*/

	if (node->streamorder < min_stream) { return true; }

	return false;
}

Atlas::Atlas()
{
	m_heightmap.resize(1024, 1024, util::COLORSPACE_GRAYSCALE);
}
	
Atlas::~Atlas()
{
	clear();
}

void Atlas::generate(int seed, const geom::Rectangle &bounds, const AtlasParameters &parameters)
{
	// clear all data
	clear();

	// generate the graph
	std::mt19937 gen(seed);
	
	glm::vec2 scale = bounds.max - bounds.min;
	std::vector<glm::vec2> points;

	PoissonGenerator::DefaultPRNG PRNG(seed);
	const auto positions = PoissonGenerator::generatePoissonPoints(8000, PRNG, false);
	for (const auto &position : positions) {
		glm::vec2 point = { scale.x * position.x, scale.y * position.y };
		point += bounds.min;
		points.push_back(point);
	}

	m_graph.generate(points, bounds, 2);

	// create tile map
	m_tiles.resize(m_graph.cells.size());
	for (const auto &cell : m_graph.cells) {
		Tile tile;
		tile.index = cell.index;
		m_tiles[cell.index] = tile;
	}
	// borders
	m_borders.resize(m_graph.edges.size());
	for (const auto &edge : m_graph.edges) {
		Border border;
		border.index = edge.index;
		m_borders[edge.index] = border;
	}
	// corners
	m_corners.resize(m_graph.vertices.size());
	for (const auto &vertex : m_graph.vertices) {
		Corner corner;
		corner.index = vertex.index;
		m_corners[vertex.index] = corner;
	}

	// find edge cases
	for (auto &border : m_borders) {
		const auto &edge = m_graph.edges[border.index];
		if (edge.left_cell == edge.right_cell) {
			border.flags |= TILE_FLAG_FRONTIER;
			m_tiles[edge.left_cell->index].flags |= TILE_FLAG_FRONTIER;
			m_corners[edge.left_vertex->index].flags |= TILE_FLAG_FRONTIER;
			m_corners[edge.right_vertex->index].flags |= TILE_FLAG_FRONTIER;
		}
	}

	// create height map
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(parameters.noise_frequency);
	fastnoise.SetFractalOctaves(parameters.noise_octaves);
	fastnoise.SetFractalLacunarity(parameters.noise_lacunarity);
	fastnoise.SetPerturbFrequency(parameters.perturb_frequency);
	fastnoise.SetGradientPerturbAmp(parameters.perturb_amp);

	for (int i = 0; i < m_heightmap.width(); i++) {
		for (int j = 0; j < m_heightmap.height(); j++) {
			float x = i; float y = j;
			fastnoise.GradientPerturbFractal(x, y);
			float height = 0.5f * (fastnoise.GetNoise(x, y) + 1.f);
			m_heightmap.plot(i, j, util::CHANNEL_RED, 255 * height);
		}
	}

	for (auto &tile : m_tiles) {
		glm::vec2 center = m_graph.cells[tile.index].center;
		float x = center.x / bounds.max.x; 
		float y = center.y / bounds.max.y;
		tile.height = m_heightmap.sample_relative(x, y, util::CHANNEL_RED);
		if (tile.height > parameters.mountains) {
			tile.relief = ReliefType::MOUNTAINS;
		} else if (tile.height > parameters.hills) {
			tile.relief = ReliefType::HILLS;
		} else if (tile.height > parameters.lowland) {
			tile.relief = ReliefType::LOWLAND;
		} else {
			tile.relief = ReliefType::SEABED;
		}
	}

	// relief floodfill
	floodfill_relief(64, ReliefType::SEABED, ReliefType::LOWLAND);
	floodfill_relief(4, ReliefType::MOUNTAINS, ReliefType::HILLS);

	remove_echoriads();

	mark_islands(64);

	mark_coasts();

	form_rivers();
}
	
void Atlas::clear()
{
	m_tiles.clear();
	m_corners.clear();
	m_borders.clear();

	delete_basins();

	m_graph.clear();

	m_heightmap.wipe();
}
	
const geom::VoronoiGraph& Atlas::graph() const
{
	return m_graph;
}

const std::vector<Tile>& Atlas::tiles() const
{
	return m_tiles;
}

const std::vector<Corner>& Atlas::corners() const
{
	return m_corners;
}

const std::vector<Border>& Atlas::borders() const
{
	return m_borders;
}
	
const util::Image<uint8_t>& Atlas::heightmap() const
{
	return m_heightmap;
}

const Tile* Atlas::tile_at(const glm::vec2 &position) const
{
	auto search = m_graph.cell_at(position);

	if (search.found) {
		return &m_tiles[search.cell->index];
	} 

	return nullptr;
}
	
glm::vec2 Atlas::tile_center(uint32_t index) const
{
	return m_graph.cells[index].center;
}
		
void Atlas::floodfill_relief(unsigned max_size, ReliefType target, ReliefType replacement)
{
	const auto &cells = m_graph.cells;

	std::unordered_map<uint32_t, bool> visited;

	for (auto &root : m_tiles) {
		std::vector<Tile*> marked;
		if (visited[root.index] == false && root.relief == target) {
			// breadth first search
			visited[root.index] = true;
			std::queue<uint32_t> queue;
			queue.push(root.index);
			
			while (!queue.empty()) {
				uint32_t node = queue.front();
				queue.pop();

				marked.push_back(&m_tiles[node]);

				for (const auto &cell : cells[node].neighbors) {
					Tile *neighbor = &m_tiles[cell->index];
					if (visited[neighbor->index] == false) {
						visited[neighbor->index] = true;
						if (neighbor->relief == target) {
							queue.push(neighbor->index);
						}
					}
				}
			}
		}

		// now that the size is known replace with target
		if (marked.size() > 0 && marked.size() < max_size) {
			for (Tile *tile : marked) {
				tile->relief = replacement;
			}
		}
	}
}
	
void Atlas::remove_echoriads()
{
	const auto &cells = m_graph.cells;

	std::unordered_map<uint32_t, bool> visited;
	
	for (auto &root : m_tiles) {
		std::vector<Tile*> marked;
		if (visited[root.index] == false && walkable_tile(&root) == true) {
			bool found_water = false;
			visited[root.index] = true;
			std::queue<Tile*> queue;
			queue.push(&root);
			
			while (!queue.empty()) {
				Tile *tile = queue.front();
				queue.pop();

				marked.push_back(tile);
				
				for (const auto &cell : cells[tile->index].neighbors) {
					Tile *neighbor = &m_tiles[cell->index];
					if (neighbor->relief == ReliefType::SEABED) {
						found_water = true;
						break;
					}
					if (visited[neighbor->index] == false) {
						visited[neighbor->index] = true;
						if (walkable_tile(neighbor)) {
							queue.push(neighbor);
						}
					}
				}
			}

			if (found_water == false) {
				for (Tile *tile : marked) {
					tile->relief = ReliefType::MOUNTAINS;
				}
			}
		}
	}
}
	
void Atlas::mark_islands(unsigned max_size)
{
	const auto &cells = m_graph.cells;

	std::unordered_map<uint32_t, bool> visited;
	
	for (auto &root : m_tiles) {
		std::vector<Tile*> marked;
		if (visited[root.index] == false && walkable_tile(&root) == true) {
			bool found_water = false;
			visited[root.index] = true;
			std::queue<Tile*> queue;
			queue.push(&root);
			
			while (!queue.empty()) {
				Tile *tile = queue.front();
				queue.pop();

				marked.push_back(tile);
				
				for (const auto &cell : cells[tile->index].neighbors) {
					Tile *neighbor = &m_tiles[cell->index];
					if (neighbor->relief == ReliefType::SEABED) {
						found_water = true;
					}
					if (visited[neighbor->index] == false) {
						visited[neighbor->index] = true;
						if (walkable_tile(neighbor)) {
							queue.push(neighbor);
						}
					}
				}
			}

			if (found_water == true && marked.size() < max_size) {
				for (Tile *tile : marked) {
					tile->flags |= TILE_FLAG_ISLAND;
				}
			}
		}
	}
}
	
void Atlas::mark_coasts()
{
	for (auto &tile : m_tiles) {
		bool local_land = tile.relief != ReliefType::SEABED;
		for (const auto &cell : m_graph.cells[tile.index].neighbors) {
			Tile *neighbor = &m_tiles[cell->index];
			bool neighbor_land = neighbor->relief != ReliefType::SEABED;
			if (local_land ^ neighbor_land) {
				tile.flags |= TILE_FLAG_COAST;
			}
		}
	}

	for (auto &border : m_borders) {
		const auto &edge = m_graph.edges[border.index];
		bool left_land = m_tiles[edge.left_cell->index].relief != ReliefType::SEABED;
		bool right_land = m_tiles[edge.right_cell->index].relief != ReliefType::SEABED;
		if (left_land ^ right_land) {
			border.flags |= TILE_FLAG_COAST;
			m_corners[edge.left_vertex->index].flags |= TILE_FLAG_COAST;
			m_corners[edge.right_vertex->index].flags |= TILE_FLAG_COAST;
		}
	}
}

void Atlas::form_rivers()
{
	// construct the drainage basin candidate graph
	// only land corners not on the edge of the map can be candidates for the graph
	std::vector<const Corner*> candidates;
	for (auto &corner : m_corners) {
		const auto &vertex = m_graph.vertices[corner.index];
		bool frontier = corner.flags & TILE_FLAG_FRONTIER;
		if (!frontier) {
			// find if corner touches land
			for (const auto &cell : vertex.cells) {
				const auto &tile = m_tiles[cell->index];
				if (tile.relief != ReliefType::SEABED) {
					candidates.push_back(&corner);
					corner.flags |= TILE_FLAG_RIVER;
					break;
				}
			}
		}
	}

	form_drainage_basins(candidates);

	trim_river_basins(3);
	//
	// after trimming make sure river properties of corners are correct
	for (auto &corner : m_corners) { 
		corner.flags &= ~TILE_FLAG_RIVER;
	}

	assign_rivers();
}

void Atlas::form_drainage_basins(const std::vector<const Corner*> &candidates)
{
	struct Meta {
		bool visited = false;
		int elevation = 0;
		int score = 0;
	};
	
	std::unordered_map<const Corner*, Meta> lookup;

	// add starting weight based on elevation
	// corners with higher elevation will get a higher weight
	for (auto node : candidates) {
		int weight = 0;
		const auto &vertex = m_graph.vertices[node->index];
		for (const auto &cell : vertex.cells) {
			const auto &tile = m_tiles[cell->index];
			if (tile.relief == ReliefType::HILLS) {
				weight += 3;
			} else if (tile.relief == ReliefType::MOUNTAINS) {
				weight += 4;
			}
		}
		Meta data = { false, weight, 0 };
		lookup[node] = data;
	}

	// breadth first search to start the drainage basin
	// assign scores
	for (auto root : candidates) {
		if (root->flags & TILE_FLAG_COAST) {
			std::queue<const Corner*> frontier;
			lookup[root].visited = true;
			frontier.push(root);
			while (!frontier.empty()) {
				const Corner *corner = frontier.front();
				frontier.pop();
				Meta &data = lookup[corner];
				int depth = data.score + data.elevation + 1;
				for (const auto &vertex : m_graph.vertices[corner->index].adjacent) {
					const Corner *neighbor = &m_corners[vertex->index];
					bool river = neighbor->flags & TILE_FLAG_RIVER;
					bool coast = neighbor->flags & TILE_FLAG_COAST;
					if (river && !coast) {
						Meta &neighbor_data = lookup[neighbor];

						if (!neighbor_data.visited) {
							neighbor_data.visited = true;
							neighbor_data.score = depth;
							frontier.push(neighbor);
						} else if (neighbor_data.score > depth && neighbor_data.elevation >= data.elevation) {
							neighbor_data.score = depth;
							frontier.push(neighbor);
						}
					}
				}
			}
		}
	}

	// reset visited
	for (auto node : candidates) {
		lookup[node].visited = false;
	}

	// create the drainage basin binary trees
	for (auto root : candidates) {
		if (root->flags & TILE_FLAG_COAST) {
			lookup[root].visited = true;
			// new drainage basin
			DrainageBasin basin = {};
			basin.mouth = create_branch(root);
			std::queue<RiverBranch*> frontier;
			frontier.push(basin.mouth);
			while (!frontier.empty()) {
				RiverBranch *fork = frontier.front(); frontier.pop();
				const Corner *corner = fork->confluence;
				Meta &corner_data = lookup[corner];
				for (const auto &vertex : m_graph.vertices[corner->index].adjacent) {
					const Corner *neighbor = &m_corners[vertex->index];
					Meta &neighbor_data = lookup[neighbor];
					bool coast = neighbor->flags & TILE_FLAG_COAST;
					if (!neighbor_data.visited && !coast) {
						if (neighbor_data.score > corner_data.score && neighbor_data.elevation >= corner_data.elevation) {
							neighbor_data.visited = true;
							// create a new branch
							RiverBranch *child = create_branch(neighbor);
							frontier.push(child);
							if (fork->left == nullptr) {
								fork->left = child;
							} else if (fork->right == nullptr) {
								fork->right = child;
							}
						}
					}
				}
			}

			basins.push_back(basin);
		}
	}

	// assign stream order numbers
	for (auto &basin : basins) {
		stream_postorder(&basin);
	}
}

void Atlas::trim_river_basins(size_t min)
{
	// prune binary tree branch if the stream order is too low
	for (auto it = basins.begin(); it != basins.end(); ) {
		DrainageBasin &bas = *it;
		std::queue<RiverBranch*> queue;
		queue.push(bas.mouth);
		while (!queue.empty()) {
			RiverBranch *cur = queue.front(); queue.pop();
			if (cur->right != nullptr) {
				if (prunable(cur->right, min)) {
					prune_branches(cur->right);
					cur->right = nullptr;
				} else {
					queue.push(cur->right);
				}
			}
			if (cur->left != nullptr) {
				if (prunable(cur->left, min)) {
					prune_branches(cur->left);
					cur->left = nullptr;
				} else {
					queue.push(cur->left);
				}
			}
		}
		if (bas.mouth->right == nullptr && bas.mouth->left == nullptr) {
			delete bas.mouth;
			bas.mouth = nullptr;
			it = basins.erase(it);
		} else {
			++it;
		}
	}
}

void Atlas::trim_stubby_rivers(uint8_t min_branch, uint8_t min_basin)
{
}

// assign river data from basins to the graph data
void Atlas::assign_rivers()
{
	// link the borders with the river corners
	std::map<std::pair<uint32_t, uint32_t>, Border*> link;
	for (auto &border : m_borders) {
		border.flags &= ~TILE_FLAG_RIVER;
		const auto &edge = m_graph.edges[border.index];
		link[std::minmax(edge.left_vertex->index, edge.right_vertex->index)] = &border;
	}

	for (const auto &basin : basins) {
		if (basin.mouth) {
			std::queue<RiverBranch*> queue;
			queue.push(basin.mouth);
			while (!queue.empty()) {
				RiverBranch *cur = queue.front(); queue.pop();
				m_corners[cur->confluence->index].flags |= TILE_FLAG_RIVER;
				m_corners[cur->confluence->index].river_depth = cur->depth;
				if (cur->right != nullptr) {
					Border *bord = link[std::minmax(cur->confluence->index, cur->right->confluence->index)];
					if (bord) { 
						bord->flags |= TILE_FLAG_RIVER;
					}
					queue.push(cur->right);
				}
				if (cur->left != nullptr) {
					Border *bord = link[std::minmax(cur->confluence->index, cur->left->confluence->index)];
					if (bord) { 
						bord->flags |= TILE_FLAG_RIVER;
					}
					queue.push(cur->left);
				}
			}

		}
	}
}
	
void Atlas::delete_basins()
{
	for (auto &basin : basins) {
		delete_basin(&basin);
	}

	basins.clear();
}

bool walkable_tile(const Tile *tile)
{
	return (tile->relief == ReliefType::LOWLAND || tile->relief == ReliefType::HILLS);
}
