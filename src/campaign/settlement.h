
class Settlement {
public:
	Settlement();
public:
	uint32_t faction() const;
	uint32_t home_tile() const;
	const std::vector<uint32_t>& tiles() const;
public:
	void set_position(const glm::vec3 &position);
	void set_faction(uint32_t faction);
	void set_home_tile(uint32_t tile);
	void add_tile(uint32_t tile);
	void sync();
public:
	const geom::Transform* transform() const;
	const fysx::TriggerSphere* trigger() const;
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_faction, m_home_tile, m_tiles, m_transform->position, m_transform->rotation, m_transform->scale);
	}
private:
	std::unique_ptr<geom::Transform> m_transform;
	std::unique_ptr<fysx::TriggerSphere> m_trigger;
	uint32_t m_faction = 0;
	uint32_t m_home_tile = 0;
	std::vector<uint32_t> m_tiles;
};

class SettlementController {
public:
	std::vector<std::unique_ptr<Settlement>> settlements;
};
