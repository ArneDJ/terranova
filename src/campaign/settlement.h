
class Settlement : public CampaignEntity {
public:
	uint32_t tile = 0; // the tile this settlement is placed on
	uint32_t faction = 0; // the faction this settlement belongs to
	uint32_t fiefdom = 0;
public:
	uint8_t size = 1;
	bool walled = false;
public:
	uint32_t troop_count = 0;
public:
	std::unique_ptr<fysx::TriggerSphere> trigger;
public:
	const gfx::Model *model = nullptr; // visual 3D model
	const gfx::Model *wall_model = nullptr;
public:
	Settlement();
//public:
	//glm::vec2 map_position() const;
public:
	void set_position(const glm::vec3 &position);
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			id, tile, faction, fiefdom, size, walled,
			transform.position, transform.rotation, transform.scale,
			name,
			ticks,
			troop_count
		);
	}
};
/*
class Village : public Settlement {
};
*/

class Town : public Settlement {
};

// consists of a governing town and the tiles it occupies
class Fiefdom {
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
	std::unordered_map<uint32_t, std::unique_ptr<Fiefdom>> fiefdoms;
	std::unordered_map<uint32_t, uint32_t> tile_owners; // left: tile ID, right: fiefdom ID
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(towns, fiefdoms, tile_owners);
	}
public:
	void clear()
	{
		towns.clear();
		fiefdoms.clear();
		tile_owners.clear();
	}
};
