
class Faction {
public:
	bool player_controlled = false;
	uint32_t capital_id = 0; // capital town
	std::list<uint32_t> towns; // towns owned by this faction
	bool expanding = false;
public:
	uint32_t id() const;
	glm::vec3 color() const;
	int gold() const;
public:
	void set_color(const glm::vec3 &color);
	void set_id(uint32_t id);
public:
	void add_gold(int amount);
	void add_town(uint32_t town);
	void remove_town(uint32_t town);
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_id, m_color, capital_id, m_gold, towns, player_controlled);
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
	void add_expand_request(uint32_t faction_id);
	uint32_t top_request();
private:
	std::unordered_map<uint32_t, bool> m_desirable_tiles;
	std::queue<uint32_t> m_expansion_requests; // queue with ids of factions that request expansion
	uint64_t m_internal_ticks = 0;
private:
	void target_town_tiles(const Tile &tile, const Atlas &atlas, int radius, std::unordered_map<uint32_t, bool> &visited, std::unordered_map<uint32_t, uint32_t> &depth);
};
