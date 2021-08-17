
enum class ReliefType : uint8_t {
	SEABED,
	LOWLAND,
	HILLS,
	MOUNTAINS
};

struct Border {
	uint32_t index;
	bool frontier = true;
};

struct Corner {
	uint32_t index;
	bool frontier = true;
};

struct Tile {
	uint32_t index;
	uint8_t height = 0;
	ReliefType relief = ReliefType::SEABED;
};

template <class Archive>
void serialize(Archive &archive, Border &border)
{
	archive(border.index, border.frontier);
}

template <class Archive>
void serialize(Archive &archive, Corner &corner)
{
	archive(corner.index, corner.frontier);
}

template <class Archive>
void serialize(Archive &archive, Tile &tile)
{
	archive(tile.index, tile.height, tile.relief);
}

struct AtlasParameters {
	uint8_t lowland = 114;
	uint8_t hills = 147;
	uint8_t mountains = 168;
	float noise_frequency = 0.002f;
	uint8_t noise_octaves = 6;
	float noise_lacunarity = 2.5f;
	float perturb_frequency = 0.002f;
	float perturb_amp = 300.f;
};

// generates a tile map with geography data (relief, temperatures, ...)
class Atlas {
public:
	void generate(int seed, const geom::Rectangle &bounds, const AtlasParameters &parameters);
public:
	const geom::VoronoiGraph& graph() const;
	const std::vector<Tile>& tiles() const;
	const std::vector<Corner>& corners() const;
	const std::vector<Border>& borders() const;
	const Tile* tile_at(const glm::vec2 &position) const;
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_graph, m_tiles, m_corners, m_borders);
	}
private:
	geom::VoronoiGraph m_graph;
	std::vector<Tile> m_tiles;
	std::vector<Corner> m_corners;
	std::vector<Border> m_borders;
private:
	void clear();
};
