#include <vector>
#include <random>
#include <memory>
#include <queue>
#include <list>
#include <chrono>
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
	return (node->streamorder < min_stream);
}

Atlas::Atlas()
{
	const uint16_t size = 2048;
	m_heightmap.resize(size, size, util::COLORSPACE_GRAYSCALE);
	m_normalmap.resize(size, size, util::COLORSPACE_RGB);
	m_mask.resize(size, size, util::COLORSPACE_GRAYSCALE);
}
	
Atlas::~Atlas()
{
	clear();
}

void Atlas::generate(int seed, const geom::Rectangle &bounds, const AtlasParameters &parameters)
{
	m_bounds = bounds;
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
			border.flags |= BORDER_FLAG_FRONTIER;
			m_tiles[edge.left_cell->index].flags |= TILE_FLAG_FRONTIER;
			m_corners[edge.left_vertex->index].flags |= CORNER_FLAG_FRONTIER;
			m_corners[edge.right_vertex->index].flags |= CORNER_FLAG_FRONTIER;
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

	/*
	for (int i = 0; i < m_heightmap.width(); i++) {
		for (int j = 0; j < m_heightmap.height(); j++) {
			float x = i; float y = j;
			fastnoise.GradientPerturbFractal(x, y);
			float height = 0.5f * (fastnoise.GetNoise(x, y) + 1.f);
			m_heightmap.plot(i, j, util::CHANNEL_RED, height);
		}
	}
	*/

	for (auto &tile : m_tiles) {
		glm::vec2 center = m_graph.cells[tile.index].center;
		//float x = center.x / bounds.max.x; 
		//float y = center.y / bounds.max.y;
		//tile.height = 255 * m_heightmap.sample_relative(x, y, util::CHANNEL_RED);
		float x = center.x; float y = center.y;
		fastnoise.GradientPerturbFractal(x, y);
		float height = 0.5f * (fastnoise.GetNoise(x, y) + 1.f);
		tile.height = 255 * height;

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

	mark_walls();

	form_rivers();

	// relief has been altered by rivers // and apply corrections
	floodfill_relief(4, ReliefType::MOUNTAINS, ReliefType::HILLS);
	mark_walls();

	// alter height map
	create_reliefmap(seed);
	river_cut_relief();
	
	//m_heightmap.normalize(util::CHANNEL_RED);
}
	
void Atlas::clear()
{
	m_tiles.clear();
	m_corners.clear();
	m_borders.clear();

	delete_basins();

	m_graph.clear();

	m_heightmap.wipe();
	m_mask.wipe();
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
	
const util::Image<float>& Atlas::heightmap() const
{
	return m_heightmap;
}

const util::Image<float>& Atlas::normalmap() const
{
	return m_normalmap;
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
	
const geom::Rectangle& Atlas::bounds() const
{
	return m_bounds;
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
			border.flags |= BORDER_FLAG_COAST;
			m_corners[edge.left_vertex->index].flags |= CORNER_FLAG_COAST;
			m_corners[edge.right_vertex->index].flags |= CORNER_FLAG_COAST;
		}
	}
}

// a wall is a border between a mountain tile and a non mountain tile
void Atlas::mark_walls()
{
	for (auto &corner : m_corners) {
		corner.flags &= ~CORNER_FLAG_WALL; // reset
		bool walkable = false;
		bool near_mountain = false;
		const auto &vertex = m_graph.vertices[corner.index];
		for (const auto &cell : vertex.cells) {
			const auto &tile = m_tiles[cell->index];
			if (tile.relief == ReliefType::MOUNTAINS)  {
				near_mountain = true;
			} else if (tile.relief == ReliefType::HILLS || tile.relief == ReliefType::LOWLAND) {
				walkable = true;
			}
		}
		bool frontier = corner.flags & CORNER_FLAG_FRONTIER;
		if (near_mountain && walkable) {
			corner.flags |= CORNER_FLAG_WALL;
		}
		if (near_mountain && frontier) {
			corner.flags |= CORNER_FLAG_WALL;
		}
	}

	for (auto &border : m_borders) {
		border.flags &= ~BORDER_FLAG_WALL; // reset
		const auto &edge = m_graph.edges[border.index];
		auto &left = m_tiles[edge.left_cell->index];
		auto &right = m_tiles[edge.right_cell->index];
		bool frontier = border.flags & BORDER_FLAG_FRONTIER;
		if (frontier && (left.relief == ReliefType::MOUNTAINS || right.relief == ReliefType::MOUNTAINS)) {
			border.flags |= BORDER_FLAG_WALL;
		} else if ((left.relief == ReliefType::MOUNTAINS) ^ (right.relief == ReliefType::MOUNTAINS)) {
			border.flags |= BORDER_FLAG_WALL;
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
		bool frontier = corner.flags & CORNER_FLAG_FRONTIER;
		if (!frontier) {
			// find if corner touches land
			for (const auto &cell : vertex.cells) {
				const auto &tile = m_tiles[cell->index];
				if (tile.relief != ReliefType::SEABED) {
					candidates.push_back(&corner);
					corner.flags |= CORNER_FLAG_RIVER;
					break;
				}
			}
		}
	}

	form_drainage_basins(candidates);

	// first river assign
	assign_rivers();

	// river basins will erode some mountains
	river_erode_mountains(2);

	trim_drainage_basins(3);

	// second river assign
	assign_rivers();

	trim_rivers();

	prune_stubby_rivers(3, 4);

	// last river assign
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
		if (root->flags & CORNER_FLAG_COAST) {
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
					bool river = neighbor->flags & CORNER_FLAG_RIVER;
					bool coast = neighbor->flags & CORNER_FLAG_COAST;
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
		if (root->flags & CORNER_FLAG_COAST) {
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
					bool coast = neighbor->flags & CORNER_FLAG_COAST;
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
	
void Atlas::river_erode_mountains(size_t min)
{
	for (auto it = basins.begin(); it != basins.end(); ) {
		DrainageBasin &bas = *it;
		std::queue<RiverBranch*> queue;
		queue.push(bas.mouth);
		while (!queue.empty()) {
			RiverBranch *cur = queue.front(); queue.pop();

			if (cur->streamorder > min) {
				const auto &vertex = m_graph.vertices[cur->confluence->index];
				for (const auto &cell : vertex.cells) {
					auto &tile = m_tiles[cell->index];
					if (tile.relief == ReliefType::MOUNTAINS) {
						tile.relief = ReliefType::HILLS;
					}
				}
			}

			if (cur->right != nullptr) {
				queue.push(cur->right);
			}
			if (cur->left != nullptr) {
				queue.push(cur->left);
			}
		}
		++it;
	}
}
	
void Atlas::trim_drainage_basins(size_t min)
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
	
void Atlas::trim_rivers()
{
	// remove rivers too close to each other
	static const float MIN_RIVER_DISTANCE = 4.F;
	for (auto &border : m_borders) {
		if (!(border.flags & BORDER_FLAG_RIVER)) {
			const auto &edge = m_graph.edges[border.index];
			// river with the smallest stream order is trimmed
			// if they have the same stream order do a coin flip
			auto &left = m_corners[edge.left_vertex->index];
			auto &right = m_corners[edge.right_vertex->index];
			if ((left.flags & CORNER_FLAG_RIVER) && (right.flags & CORNER_FLAG_RIVER)) {
				float d = glm::distance(edge.left_vertex->position, edge.right_vertex->position);
				if (d < MIN_RIVER_DISTANCE) {
					if (left.river_depth > right.river_depth) {
						right.flags &= ~CORNER_FLAG_RIVER;
					} else {
						left.flags &= ~CORNER_FLAG_RIVER;
					}
				}
			}
		}
	}

	// remove rivers too close to map edges or mountains
	for (auto &corner : m_corners) {
		if (corner.flags & CORNER_FLAG_RIVER) {
			const auto &vertex = m_graph.vertices[corner.index];
			for (const auto &adjacent : vertex.adjacent) {
				const auto &neighbor = m_corners[adjacent->index];
				if (neighbor.flags & CORNER_FLAG_WALL) {
					corner.flags &= ~CORNER_FLAG_RIVER;
					break;
				}
			}
		}
	}

	// do the actual pruning in the basin
	for (auto it = basins.begin(); it != basins.end(); ) {
		DrainageBasin &basin = *it;
		std::queue<RiverBranch*> queue;
		queue.push(basin.mouth);
		while (!queue.empty()) {
			RiverBranch *cur = queue.front();
			queue.pop();

			if (cur->right != nullptr) {
				if ((cur->right->confluence->flags & CORNER_FLAG_RIVER) == false) {
					prune_branches(cur->right);
					cur->right = nullptr;
				} else {
					queue.push(cur->right);
				}
			}
			if (cur->left != nullptr) {
				if ((cur->left->confluence->flags & CORNER_FLAG_RIVER) == false) {
					prune_branches(cur->left);
					cur->left = nullptr;
				} else {
					queue.push(cur->left);
				}
			}
		}
		if (basin.mouth->right == nullptr && basin.mouth->left == nullptr) {
			delete basin.mouth;
			basin.mouth = nullptr;
			it = basins.erase(it);
		} else {
			++it;
		}
	}
}

void Atlas::prune_stubby_rivers(uint8_t min_branch, uint8_t min_basin)
{
	std::unordered_map<const RiverBranch*, int> depth;
	std::unordered_map<const RiverBranch*, bool> removable;
	std::unordered_map<RiverBranch*, RiverBranch*> parents;
	std::vector<RiverBranch*> endnodes;

	// find river end nodes
	for (auto it = basins.begin(); it != basins.end(); ) {
		DrainageBasin &bas = *it;
		std::queue<RiverBranch*> queue;
		queue.push(bas.mouth);
		parents[bas.mouth] = nullptr;
		removable[bas.mouth] = false;
		while (!queue.empty()) {
			RiverBranch *cur = queue.front();
			depth[cur] = -1;
			queue.pop();
			if (cur->right == nullptr && cur->left == nullptr) {
				endnodes.push_back(cur);
				depth[cur] = 0;
			} else {
				if (cur->right != nullptr) {
					queue.push(cur->right);
					parents[cur->right] = cur;
				}
				if (cur->left != nullptr) {
					queue.push(cur->left);
					parents[cur->left] = cur;
				}
			}

		}
		++it;
	}

	// starting from end nodes assign depth to nodes until they reach a branch
	for (auto &node : endnodes) {
		std::queue<RiverBranch*> queue;
		queue.push(node);
		while (!queue.empty()) {
			RiverBranch *cur = queue.front();
			queue.pop();
			RiverBranch *parent = parents[cur];
			if (parent) {
				depth[parent] = depth[cur] + 1;
				if (parent->left != nullptr && parent->right != nullptr) {
				// reached a branch
					if (depth[cur] > -1 && depth[cur] < min_branch) {
						prune_branches(cur);
						if (cur == parent->left) {
							parent->left = nullptr;
						} else if (cur == parent->right) {
							parent->right = nullptr;
						}
					}
				} else {
					queue.push(parent);
				}
			} else if (depth[cur] < min_basin) {
			// reached the river mouth
			// river is simply too small so mark it for deletion
				removable[cur] = true;
			}
		}
	}

	// remove river basins if they are too small
	for (auto it = basins.begin(); it != basins.end(); ) {
		DrainageBasin &bas = *it;
		if (removable[bas.mouth]) {
			delete_basin(&bas);
			it = basins.erase(it);
		} else {
			it++;
		}
	}
}

// assign river data from basins to the graph data
void Atlas::assign_rivers()
{
	// reset river flags
	for (auto &corner : m_corners) { 
		corner.flags &= ~CORNER_FLAG_RIVER;
	}
	for (auto &border : m_borders) { 
		border.flags &= ~BORDER_FLAG_RIVER;
	}
	for (auto &tile : m_tiles) { 
		tile.flags &= ~TILE_FLAG_RIVER;
	}

	// link the borders with the river corners
	std::map<std::pair<uint32_t, uint32_t>, Border*> link;
	for (auto &border : m_borders) {
		border.flags &= ~BORDER_FLAG_RIVER;
		const auto &edge = m_graph.edges[border.index];
		link[std::minmax(edge.left_vertex->index, edge.right_vertex->index)] = &border;
	}

	for (const auto &basin : basins) {
		if (basin.mouth) {
			std::queue<RiverBranch*> queue;
			queue.push(basin.mouth);
			while (!queue.empty()) {
				RiverBranch *cur = queue.front(); queue.pop();
				m_corners[cur->confluence->index].flags |= CORNER_FLAG_RIVER;
				m_corners[cur->confluence->index].river_depth = cur->depth;
				if (cur->right != nullptr) {
					Border *bord = link[std::minmax(cur->confluence->index, cur->right->confluence->index)];
					if (bord) { 
						bord->flags |= BORDER_FLAG_RIVER;
					}
					queue.push(cur->right);
				}
				if (cur->left != nullptr) {
					Border *bord = link[std::minmax(cur->confluence->index, cur->left->confluence->index)];
					if (bord) { 
						bord->flags |= BORDER_FLAG_RIVER;
					}
					queue.push(cur->left);
				}
			}

		}
	}

	for (auto &tile : m_tiles) {
		const auto &cell = m_graph.cells[tile.index];
		for (auto &edge : cell.edges) {
			const auto &border = m_borders[edge->index];
			if (border.flags & BORDER_FLAG_RIVER) { 
				tile.flags |= TILE_FLAG_RIVER;
				break;
			}
		}
	}
}
	
void Atlas::form_mountain_ridges()
{
	// create initial candidate network
	// only corners next to mountains are part of the network
	std::vector<const Corner*> candidates;
	std::unordered_map<uint32_t, bool> touches_mountain;
	for (const auto &corner : m_corners) {
		touches_mountain[corner.index] = false;
		const auto &vertex = m_graph.vertices[corner.index];
		// find if corner touches mountains
		for (const auto &cell : vertex.cells) {
			const auto &tile = m_tiles[cell->index];
			if (tile.relief == ReliefType::MOUNTAINS) {
				candidates.push_back(&corner);
				touches_mountain[corner.index] = true;
				break;
			}
		}
	}

	struct Meta {
		bool visited = false;
		int score = 0;
	};
	std::unordered_map<const Corner*, Meta> lookup;

	// starting scores
	for (auto node : candidates) {
		Meta data = { false, 0 };
		lookup[node] = data;
	}

	// do breadth first search for each mountain corner to find the closest "exit" out of the mountains
	// this creates something similar to a drainage network
	// this drainage network is used to define the mountain ridges
	for (auto root : candidates) {
		if (root->flags & CORNER_FLAG_WALL) {
			std::queue<const Corner*> frontier;
			lookup[root].visited = true;
			frontier.push(root);
			while (!frontier.empty()) {
				const Corner *corner = frontier.front();
				frontier.pop();
				Meta &data = lookup[corner];
				int depth = data.score + 1;
				for (const auto &vertex : m_graph.vertices[corner->index].adjacent) {
					const Corner *neighbor = &m_corners[vertex->index];
					bool mountain = touches_mountain[neighbor->index];
					bool wall = neighbor->flags & CORNER_FLAG_WALL;
					if (mountain && !wall) {
						Meta &neighbor_data = lookup[neighbor];

						if (!neighbor_data.visited) {
							neighbor_data.visited = true;
							neighbor_data.score = depth;
							frontier.push(neighbor);
						} else if (neighbor_data.score > depth) {
							neighbor_data.score = depth;
							frontier.push(neighbor);
						}
					}
				}
			}
		}
	}

	// now that we have the scores create the mountain valley network
	// true if a border is a valley
	std::unordered_map<uint32_t, bool> valleys;

	// link the borders with the river corners
	std::map<std::pair<uint32_t, uint32_t>, uint32_t> link;
	for (const auto &border : m_borders) {
		const auto &edge = m_graph.edges[border.index];
		link[std::minmax(edge.left_vertex->index, edge.right_vertex->index)] = border.index;
	}

	// reset visited
	for (auto node : candidates) {
		lookup[node].visited = false;
	}

	// create the drainage basin binary trees
	for (auto root : candidates) {
		if (root->flags & CORNER_FLAG_WALL) {
			std::queue<const Corner*> frontier;
			lookup[root].visited = true;
			frontier.push(root);
			while (!frontier.empty()) {
				const Corner *corner = frontier.front();
				frontier.pop();
				Meta &data = lookup[corner];
				for (const auto &vertex : m_graph.vertices[corner->index].adjacent) {
					const Corner *neighbor = &m_corners[vertex->index];
					Meta &neighbor_data = lookup[neighbor];
					bool wall = neighbor->flags & CORNER_FLAG_WALL;
					if (!neighbor_data.visited && !wall) {
						if (neighbor_data.score > data.score) {
							neighbor_data.visited = true;
							frontier.push(neighbor);
							// mark valley
							auto bord = link[std::minmax(corner->index, neighbor->index)];
							valleys[bord] = true;
						}
					}
				}
			}
		}
	}

	m_mask.wipe();

	const uint8_t color = 255;

	// based on the valley network we will asign mountain ridges
	// this gives a "fractal" appearance to the mountains like in real life
	for (const auto &border : m_borders) {
		// is not a valley so we will make a mountain ridge by connecting the two tiles
		if (!(valleys[border.index])) {
			const auto &edge = m_graph.edges[border.index];
			auto &left = m_tiles[edge.left_cell->index];
			auto &right = m_tiles[edge.right_cell->index];
			// only make ridge if both are mountains
			if (left.relief == ReliefType::MOUNTAINS && right.relief == ReliefType::MOUNTAINS) {
				glm::vec2 a = m_graph.cells[left.index].center / m_bounds.max;
				glm::vec2 b = m_graph.cells[right.index].center / m_bounds.max;
				m_mask.draw_thick_line_relative(a, b, 3, util::CHANNEL_RED, color);
			}
		}
	}

	m_mask.blur(1.f);

	// finally apply the mountain ridges to the heightmap
	#pragma omp parallel for
	for (int x = 0; x < m_heightmap.width(); x++) {
		for (int y = 0; y < m_heightmap.height(); y++) {
			float height = m_heightmap.sample(x, y, util::CHANNEL_RED);
			uint8_t amp = m_mask.sample(x, y, util::CHANNEL_RED);
			if (amp) {
				height += 0.1f * (amp / 255.f);
				m_heightmap.plot(x, y, util::CHANNEL_RED, height);
			}
		}
	}
}
	
void Atlas::form_base_relief()
{
	// the mountain tiles have an amp score
	// the closer a mountain tile is to a non mountain tile the lower the score
	struct Meta {
		bool visited = false;
		int score = 0;
	};
	std::unordered_map<const Tile*, Meta> lookup;

	std::vector<const Tile*> roots; // mountain tiles to start breadth first search
	for (const auto &tile : m_tiles) {
		Meta data = { false, 0 };
		if (tile.relief == ReliefType::MOUNTAINS) {
			bool mountain_border = false;
			for (const auto &cell : m_graph.cells[tile.index].neighbors) {
				Tile *neighbor = &m_tiles[cell->index];
				if (neighbor->relief != ReliefType::MOUNTAINS) {
					mountain_border = true;
					break;
				}
			}
			if (mountain_border) {
				roots.push_back(&tile);
				data.visited = true;
			}
		}
		// starting scores
		lookup[&tile] = data;
	}


	// do breadth first search to calculate score
	for (const auto &root : roots) {
		std::queue<const Tile*> frontier;
		lookup[root].visited = true;
		frontier.push(root);
		while (!frontier.empty()) {
			const Tile *tile = frontier.front();
			frontier.pop();
			Meta &data = lookup[tile];
			int depth = data.score + 1;
			for (const auto &cell : m_graph.cells[tile->index].neighbors) {
				const Tile *neighbor = &m_tiles[cell->index];
				bool mountain = neighbor->relief == ReliefType::MOUNTAINS;
				if (mountain) {
					Meta &neighbor_data = lookup[neighbor];
					if (!neighbor_data.visited) {
						neighbor_data.visited = true;
						neighbor_data.score = depth;
						frontier.push(neighbor);
					} else if (neighbor_data.score > depth) {
						neighbor_data.score = depth;
						frontier.push(neighbor);
					}
				}
			}
		}
	}

	geom::Bounding<int> score_bounds = { 0, 5 };

	// base per tile relief heightmap
	#pragma omp parallel for
	for (const auto &tile : m_tiles) {
		float amp = 0.7f;
		// translate mountain score to amplitude
		if (tile.relief == ReliefType::MOUNTAINS) {
			Meta &data = lookup[&tile];
			// clamp the score within the bounds
			int score = glm::clamp(data.score, score_bounds.min, score_bounds.max);
			float mix = score / float(score_bounds.max);
			amp = glm::mix(0.8f, 1.f, mix);
		} else if (tile.relief == ReliefType::SEABED) {
			amp = 0.4f;
		}

		// draw the triangles
		glm::vec2 center = m_graph.cells[tile.index].center / m_bounds.max;
		const auto &cell = m_graph.cells[tile.index];
		for (auto &edge : cell.edges) {
			const auto &left_vertex = edge->left_vertex;
			const auto &right_vertex = edge->right_vertex;
			glm::vec2 a = left_vertex->position / m_bounds.max;
			glm::vec2 b = right_vertex->position / m_bounds.max;
			m_heightmap.draw_triangle_relative(center, a, b, util::CHANNEL_RED, amp * (tile.height / 255.f));
		}
	}
	
}
	
void Atlas::create_reliefmap(int seed)
{
	form_base_relief();

	form_mountain_ridges();

	m_heightmap.blur(4.f);
	
	m_mask.blur(2.f);

	// detail
	FastNoise billow;
	billow.SetSeed(seed);
	billow.SetNoiseType(FastNoise::SimplexFractal);
	billow.SetFractalType(FastNoise::Billow);
	billow.SetFrequency(0.03f);
	billow.SetFractalOctaves(6);
	billow.SetFractalLacunarity(2.5f);
	billow.SetGradientPerturbAmp(40.f);

	 // for edgy mountain peaks
	FastNoise cellnoise;
	cellnoise.SetSeed(seed);
	cellnoise.SetNoiseType(FastNoise::Cellular);
	cellnoise.SetCellularDistanceFunction(FastNoise::Euclidean);
	cellnoise.SetFrequency(0.2f);
	cellnoise.SetCellularReturnType(FastNoise::Distance2Add);
	
	#pragma omp parallel for
	for (int x = 0; x < m_heightmap.width(); x++) {
		for (int y = 0; y < m_heightmap.height(); y++) {
			uint8_t amp = m_mask.sample(x, y, util::CHANNEL_RED);
			if (amp) {
				float height = m_heightmap.sample(x, y, util::CHANNEL_RED);
				float noise = 0.5f * (billow.GetNoise(x, y) + 1.f);
				float peak = 0.08f * cellnoise.GetNoise(x, y);
				height += 0.1f * noise;
				height = glm::mix(height, height + peak, amp / 255.f);
				m_heightmap.plot(x, y, util::CHANNEL_RED, height);
			}
		}
	}

	m_heightmap.blur(1.f);
}

void Atlas::river_cut_relief()
{
	// blank canvas
	m_mask.wipe();

	// add rivers as lines to the mask
	#pragma omp parallel for
	for (const auto &border : m_borders) {
		if (border.flags & BORDER_FLAG_RIVER) {
			const auto &edge = m_graph.edges[border.index];
			const auto &left_vertex = edge.left_vertex;
			const auto &right_vertex = edge.right_vertex;
			glm::vec2 a = left_vertex->position / m_bounds.max;
			glm::vec2 b = right_vertex->position / m_bounds.max;
			//m_mask.draw_line_relative(a, b, util::CHANNEL_RED, 255);
			m_mask.draw_thick_line_relative(a, b, 1, util::CHANNEL_RED, 255);
		}
	}

	// blur the mask a bit so we have smooth transition
	m_mask.blur(1.f);

	// now alter the heightmap based on the river mask
	#pragma omp parallel for
	for (int x = 0; x < m_heightmap.width(); x++) {
		for (int y = 0; y < m_heightmap.height(); y++) {
			uint8_t masker = m_mask.sample(x, y, util::CHANNEL_RED);
			if (masker > 0) {
				float height = m_heightmap.sample(x, y, util::CHANNEL_RED);
				float erosion = glm::clamp(1.f - (masker / 255.f), 0.8f, 1.f);
				m_heightmap.plot(x, y, util::CHANNEL_RED, erosion * height);
			}
		}
	}
}
	
void Atlas::create_normalmap()
{
	float strength = 16.f;

	#pragma omp parallel for
	for (int x = 0; x < m_heightmap.width(); x++) {
		for (int y = 0; y < m_heightmap.height(); y++) {
			const glm::vec3 normal = util::filter_normal(x, y, util::CHANNEL_RED, strength, m_heightmap);
			m_normalmap.plot(x, y, util::CHANNEL_RED, normal.x);
			m_normalmap.plot(x, y, util::CHANNEL_GREEN, normal.y);
			m_normalmap.plot(x, y, util::CHANNEL_BLUE, normal.z);
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
