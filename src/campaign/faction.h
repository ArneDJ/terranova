
class Faction {
public:
	uint32_t id() const;
	glm::vec3 color() const;
public:
	void set_color(const glm::vec3 &color);
	void set_id(uint32_t id);
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_id, m_color);
	}
private:
	uint32_t m_id = 0;
	glm::vec3 m_color = {};
};

class FactionController {
public:
	//std::unordered_map<uint32_t, uint32_t> tile_owners; // left: tile ID, right: faction ID, 0 means tile is not occupied by a faction
	std::unordered_map<uint32_t, std::unique_ptr<Faction>> factions;
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(factions);
	}
public:
	void clear()
	{
		//tile_owners.clear();
		factions.clear();
	}
};
