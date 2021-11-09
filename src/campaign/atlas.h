
enum class ReliefType : uint8_t {
	SEABED,
	LOWLAND,
	HILLS,
	MOUNTAINS
};

enum TileFlags {
	TILE_FLAG_NONE = 0,
	TILE_FLAG_FRONTIER = 1 << 0,
	TILE_FLAG_RIVER = 1 << 1,
	TILE_FLAG_COAST = 1 << 2,
	TILE_FLAG_ISLAND = 1 << 3
};

enum BorderFlags {
	BORDER_FLAG_NONE = 0,
	BORDER_FLAG_FRONTIER = 1 << 0,
	BORDER_FLAG_RIVER = 1 << 1,
	BORDER_FLAG_COAST = 1 << 2,
	BORDER_FLAG_WALL = 1 << 3 // border between mountain and non mountain tile
};

enum CornerFlags {
	CORNER_FLAG_NONE = 0,
	CORNER_FLAG_FRONTIER = 1 << 0,
	CORNER_FLAG_RIVER = 1 << 1,
	CORNER_FLAG_COAST = 1 << 2,
	CORNER_FLAG_WALL = 1 << 3
};

struct Border {
	uint32_t index;
	uint32_t flags = 0;
};

struct Corner {
	uint32_t index;
	uint32_t flags = 0;
	int river_depth = 0;
};

struct Tile {
	uint32_t index;
	uint8_t height = 0;
	uint32_t flags = 0;
	ReliefType relief = ReliefType::SEABED;
};

struct RiverBranch {
	const Corner *confluence = nullptr;
	RiverBranch *left = nullptr;
	RiverBranch *right = nullptr;
	int streamorder = 0;
	int depth = 0;
};

struct DrainageBasin {
	RiverBranch *mouth = nullptr; // binary tree root
	size_t height = 0; // binary tree height
};

template <class Archive>
void serialize(Archive &archive, Border &border)
{
	archive(border.index, border.flags);
}

template <class Archive>
void serialize(Archive &archive, Corner &corner)
{
	archive(corner.index, corner.flags, corner.river_depth);
}

template <class Archive>
void serialize(Archive &archive, Tile &tile)
{
	archive(tile.index, tile.height, tile.relief, tile.flags);
}

struct AtlasParameters {
	uint8_t lowland = 114;
	uint8_t hills = 147;
	uint8_t mountains = 168;
	float noise_frequency = 0.002f;
	uint8_t noise_octaves = 6;
	float noise_lacunarity = 2.5f;
	float perturb_frequency = 0.002f;
	float perturb_amp = 250.f;
};

// generates a tile map with geography data (relief, temperatures, ...)
class Atlas {
public:
	Atlas();
	~Atlas();
public:
	void generate(int seed, const geom::Rectangle &bounds, const AtlasParameters &parameters);
public:
	const geom::VoronoiGraph& graph() const;
	const std::vector<Tile>& tiles() const;
	const std::vector<Corner>& corners() const;
	const std::vector<Border>& borders() const;
	const util::Image<uint8_t>& heightmap() const;
	const Tile* tile_at(const glm::vec2 &position) const;
	glm::vec2 tile_center(uint32_t index) const;
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_graph, m_tiles, m_corners, m_borders, m_heightmap);
	}
private:
	geom::VoronoiGraph m_graph;
	std::vector<Tile> m_tiles;
	std::vector<Corner> m_corners;
	std::vector<Border> m_borders;
	util::Image<uint8_t> m_heightmap;
private:
	std::list<DrainageBasin> basins;
	void delete_basins();
private:
	void clear();
private: // relief stuff
	void floodfill_relief(unsigned max_size, ReliefType target, ReliefType replacement);
	void remove_echoriads();
	void mark_islands(unsigned max_size);
	void mark_coasts();
	void mark_walls();
private: // river stuff
	void form_rivers();
	void form_drainage_basins(const std::vector<const Corner*> &candidates);
	void river_erode_mountains(size_t min);
	void trim_drainage_basins(size_t min);
	void trim_rivers();
	void prune_stubby_rivers(uint8_t min_branch, uint8_t min_basin);
	void assign_rivers();
};

bool walkable_tile(const Tile *tile);
