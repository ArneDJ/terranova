
struct FactionColor {
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;
};

class Faction {
public:
	Faction(uint32_t ID)
		: m_ID(ID)
	{
	}
public:
	uint32_t ID() const { return m_ID; };
	FactionColor color() const { return m_color; };
public:
	void set_color(const FactionColor &color)
	{
		m_color = color;
	}
private:
	uint32_t m_ID = 0;
	FactionColor m_color = {};
};

class FactionController {
public:
	std::unordered_map<uint32_t, uint32_t> tile_owners; // left: tile ID, right: faction ID, 0 means tile is not occupied by a faction
	std::vector<std::unique_ptr<Faction>> factions;
};
