
struct FactionColor {
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;
};

class Faction {
public:
	std::vector<uint32_t> frontier_tiles;
public:
	uint32_t ID() const { return m_ID; };
	FactionColor color() const { return m_color; };
public:
	void set_color(const FactionColor &color)
	{
		m_color = color;
	}
	void set_ID(uint32_t ID) 
	{ 
		m_ID = ID; 
	};
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_ID, m_color.red, m_color.green, m_color.blue, frontier_tiles);
	}
private:
	uint32_t m_ID = 0;
	FactionColor m_color = {};
};

class FactionController {
public:
	std::unordered_map<uint32_t, uint32_t> tile_owners; // left: tile ID, right: faction ID, 0 means tile is not occupied by a faction
	std::vector<std::unique_ptr<Faction>> factions;
	float time_slot = 0.f;
};
