
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

class CampaignModule {
public:
	BoardModule board_module;
};
