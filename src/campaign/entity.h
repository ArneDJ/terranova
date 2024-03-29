
class CampaignEntity {
public:
	uint32_t id = 0;
	uint64_t ticks = 0; // internal ticks to track age
	bool visible = false;
	geom::Transform transform;
	std::string name = {};
public:
	const gfx::Model *model = nullptr;
public:
	glm::vec2 map_position() const
	{
		return glm::vec2(transform.position.x, transform.position.z);
	}
};
