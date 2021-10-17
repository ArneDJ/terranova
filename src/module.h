
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
};
