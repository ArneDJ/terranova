
/*
class Village {
};
*/

class Town {
public:
	Town();
public:
	uint32_t id() const;
	uint32_t faction() const;
	uint32_t tile() const;
	uint32_t county() const;
	glm::vec2 map_position() const;
public:
	const geom::Transform* transform() const;
	const fysx::TriggerSphere* trigger() const;
public:
	void set_id(uint32_t id);
	void set_position(const glm::vec3 &position);
	void set_faction(uint32_t faction);
	void set_tile(uint32_t tile);
	void set_county(uint32_t county);
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			m_id, m_tile, m_faction, m_county, 
			m_transform->position, m_transform->rotation, m_transform->scale
		);
	}
private:
	uint32_t m_id = 0;
	std::unique_ptr<geom::Transform> m_transform;
	std::unique_ptr<fysx::TriggerSphere> m_trigger;
	uint32_t m_tile = 0; // the tile this town is placed on
	uint32_t m_faction = 0; // the faction this town belongs to
	uint32_t m_county = 0;
};

// consists of a governing town and the tiles it occupies
class County {
public:
	uint32_t id() const;
	uint32_t faction() const;
	uint32_t town() const;
	const std::vector<uint32_t>& tiles() const;
public:
	void set_id(uint32_t id);
	void set_faction(uint32_t faction);
	void set_town(uint32_t town);
public:
	void add_tile(uint32_t tile);
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_id, m_town, m_faction, m_tiles);
	}
private:
	uint32_t m_id = 0;
	uint32_t m_town = 0;
	uint32_t m_faction = 0;
	std::vector<uint32_t> m_tiles;
};

class SettlementController {
public:
	std::unordered_map<uint32_t, std::unique_ptr<Town>> towns;
	std::unordered_map<uint32_t, std::unique_ptr<County>> counties;
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(towns, counties);
	}
public:
	void clear()
	{
		towns.clear();
		counties.clear();
	}
};
