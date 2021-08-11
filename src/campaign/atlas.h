
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
	archive(tile.index, tile.height);
}

// generates a tile map with geography data (relief, temperatures, ...)
class Atlas {
public:
	void generate(int seed, const geom::Rectangle &bounds);
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
