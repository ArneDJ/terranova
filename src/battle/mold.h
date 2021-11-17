
class BuildingMold {
public:
	uint32_t id = 0;
	const gfx::Model *model = nullptr;
	std::unique_ptr<fysx::CollisionMesh> collision;
public:
	BuildingMold(uint32_t id, const gfx::Model *model);
};

class Moldmaker {
};
