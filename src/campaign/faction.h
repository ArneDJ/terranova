
class Faction {
public:
	uint32_t capital_id = 0; // capital town
	std::list<uint32_t> towns; // towns owned by this faction
public:
	uint32_t id() const;
	glm::vec3 color() const;
	int gold() const;
public:
	void set_color(const glm::vec3 &color);
	void set_id(uint32_t id);
public:
	void add_gold(int amount);
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_id, m_color, capital_id, m_gold, towns);
	}
private:
	int m_gold = 100; // faction treasury
	uint32_t m_id = 0;
	glm::vec3 m_color = {};
};

class FactionController {
public:
	std::unordered_map<uint32_t, uint32_t> tile_owners; // left: tile ID, right: faction ID, 0 means tile is not occupied by a faction
	std::unordered_map<uint32_t, std::unique_ptr<Faction>> factions;
	std::vector<uint32_t> town_targets; // tiles where faction AI will try to place towns on
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(factions,  tile_owners, town_targets, m_desirable_tiles);
	}
public:
	void clear();
	void find_town_targets(const Atlas &atlas, int radius);
	uint32_t find_closest_town_target(const Atlas &atlas, Faction *faction, uint32_t origin_tile);
private:
	std::unordered_map<uint32_t, bool> m_desirable_tiles;
	std::queue<uint32_t> m_expansion_requests; // queue with ids of factions that request expansion
	uint64_t m_internal_ticks = 0;
};
