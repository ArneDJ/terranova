
class CampaignEntity {
public:
	uint32_t id = 0;
	uint64_t ticks = 0; // internal ticks to track age
	const gfx::Model *model = nullptr;
	bool visible = false;
	geom::Transform transform;
	std::string name = {};
};
