
struct HousingModule {
	geom::Bounding<uint8_t> precipitation;
	geom::Bounding<uint8_t> temperature;
	std::vector<std::string> models;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(precipitation),
			CEREAL_NVP(temperature),
			CEREAL_NVP(models)
		);
	}
};

struct FortificationModule {
	std::string segment_even;
	std::string segment_both;
	std::string segment_left;
	std::string segment_right;
	std::string tower;
	std::string ramp;
	std::string gate;

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
	std::vector<HousingModule> houses;
	FortificationModule fortification;
	std::vector<HitCapsuleModule> hitboxes;
};
