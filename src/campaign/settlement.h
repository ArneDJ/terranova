
class Settlement {
public:
	Settlement();
public:
	uint32_t faction() const;
public:
	void set_position(const glm::vec3 &position);
	void set_faction(uint32_t faction);
	void sync();
	void expand_radius();
public:
	const geom::Transform* transform() const;
	const fysx::TriggerSphere* trigger() const;
	uint32_t radius() const;
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_faction, m_tile_radius, m_transform->position, m_transform->rotation, m_transform->scale);
	}
private:
	std::unique_ptr<geom::Transform> m_transform;
	std::unique_ptr<fysx::TriggerSphere> m_trigger;
	uint32_t m_faction = 0;
	uint32_t m_tile_radius = 1;
};

class SettlementController {
public:
	std::vector<std::unique_ptr<Settlement>> settlements;
};
