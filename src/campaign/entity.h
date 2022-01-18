
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
	std::unique_ptr<gfx::Label> label;
public:
	CampaignEntity()
	{
		label = std::make_unique<gfx::Label>();
	}
};
