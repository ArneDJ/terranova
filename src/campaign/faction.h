
class Faction {
public:
	uint32_t ID() const { return m_ID; };
	glm::vec3 color() const { return m_color; };
public:
	void set_color(const glm::vec3 &color)
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
		archive(m_ID, m_color, m_desired_tiles);
	}
private:
	std::vector<uint32_t> m_desired_tiles; // desired tiles to build settlements
	uint32_t m_ID = 0;
	glm::vec3 m_color = {};
};

class FactionController {
public:
	std::unordered_map<uint32_t, uint32_t> tile_owners; // left: tile ID, right: faction ID, 0 means tile is not occupied by a faction
	std::unordered_map<uint32_t, std::unique_ptr<Faction>> factions;
	float time_slot = 0.f;
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(factions, tile_owners);
	}
public:
	void clear()
	{
		tile_owners.clear();
		factions.clear();
	}
};
