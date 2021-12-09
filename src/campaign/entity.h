
class BaseEntity {
public:
	uint32_t id = 0;
	const gfx::Model *model = nullptr;
	bool visible = false;
	geom::Transform transform;
};
