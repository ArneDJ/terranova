
struct BuildingModule {
	std::string id;
	std::string model;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(id), CEREAL_NVP(model));
	}
};

struct ArchitectureStyleModule {
	geom::Bounding<uint8_t> precipitation;
	geom::Bounding<uint8_t> temperature;
	std::vector<BuildingModule> houses;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(precipitation),
			CEREAL_NVP(temperature),
			CEREAL_NVP(houses)
		);
	}
};

struct FortificationModule {
	BuildingModule segment_even;
	BuildingModule segment_both;
	BuildingModule segment_left;
	BuildingModule segment_right;
	BuildingModule tower;
	BuildingModule ramp;
	BuildingModule gate;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(segment_even),
			CEREAL_NVP(segment_both),
			CEREAL_NVP(segment_left),
			CEREAL_NVP(segment_right),
			CEREAL_NVP(tower),
			CEREAL_NVP(ramp),
			CEREAL_NVP(gate)
		);
	}
};

struct HitCapsuleModule {
	std::string joint_a = "";
	std::string joint_b = "";
	float radius = 1.f;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(joint_a),
			CEREAL_NVP(joint_b),
			CEREAL_NVP(radius)
		);
	}
};

struct BoardModule {
	std::string marker = {};
	std::string town = {};
	std::string meeple = {};

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(marker), CEREAL_NVP(town), CEREAL_NVP(meeple));
	}
};

class Module {
public:
	BoardModule board_module;
	std::vector<ArchitectureStyleModule> architectures;
	FortificationModule fortification;
	std::vector<HitCapsuleModule> hitboxes;
};
